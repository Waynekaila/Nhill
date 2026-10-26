-- Nhill migration 0002 — business (tenant) tables.
-- Every table carries establishment_id. Cross-table references use composite
-- foreign keys (establishment_id, id) so that a row can never point to data of
-- another establishment, even through a programming error.

CREATE TABLE establishment_settings (
    establishment_id          uuid PRIMARY KEY REFERENCES establishments(id) ON DELETE CASCADE,
    allow_negative_stock      boolean NOT NULL DEFAULT false,
    tax_rounding              text NOT NULL DEFAULT 'per_line' CHECK (tax_rounding IN ('per_line', 'per_rate')),
    default_payment_terms_days integer NOT NULL DEFAULT 0 CHECK (default_payment_terms_days BETWEEN 0 AND 365),
    document_language         text NOT NULL DEFAULT 'fr' CHECK (document_language IN ('fr', 'en')),
    invoice_footer            text NOT NULL DEFAULT '',
    payment_instructions      text NOT NULL DEFAULT '',
    updated_at                timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE objects (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    purpose          text NOT NULL CHECK (purpose IN ('logo', 'receipt')),
    mime             text NOT NULL CHECK (mime IN ('image/png', 'image/jpeg', 'application/pdf')),
    byte_size        integer NOT NULL CHECK (byte_size > 0 AND byte_size <= 5242880),
    sha256           text NOT NULL,
    width            integer,
    height           integer,
    data             bytea NOT NULL,
    created_by       uuid,
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id)
);

CREATE TABLE tax_rates (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    code             text NOT NULL CHECK (code ~ '^[A-Z0-9_-]{1,16}$'),
    label            text NOT NULL,
    rate_bp          integer NOT NULL CHECK (rate_bp BETWEEN 0 AND 10000),
    kind             text NOT NULL DEFAULT 'standard' CHECK (kind IN ('standard', 'reduced', 'zero', 'exempt')),
    is_default       boolean NOT NULL DEFAULT false,
    active           boolean NOT NULL DEFAULT true,
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, code),
    UNIQUE (establishment_id, id)
);
CREATE UNIQUE INDEX tax_rates_one_default ON tax_rates (establishment_id) WHERE is_default;

CREATE TABLE product_categories (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    name             text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 80),
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id)
);
CREATE UNIQUE INDEX product_categories_name ON product_categories (establishment_id, lower(name));

CREATE TABLE products (
    id                   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id     uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    kind                 text NOT NULL DEFAULT 'good' CHECK (kind IN ('good', 'service')),
    sku                  text NOT NULL CHECK (length(btrim(sku)) BETWEEN 1 AND 40),
    barcode              text CHECK (barcode IS NULL OR barcode ~ '^[0-9A-Za-z-]{4,32}$'),
    name                 text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 200),
    description          text NOT NULL DEFAULT '',
    category_id          uuid,
    unit                 text NOT NULL DEFAULT 'unit' CHECK (length(unit) BETWEEN 1 AND 16),
    currency             char(3) NOT NULL REFERENCES currencies(code),
    purchase_price_minor bigint NOT NULL DEFAULT 0 CHECK (purchase_price_minor >= 0),
    sale_price_minor     bigint NOT NULL DEFAULT 0 CHECK (sale_price_minor >= 0),
    tax_rate_id          uuid,
    track_stock          boolean NOT NULL DEFAULT true,
    reorder_level_milli  bigint NOT NULL DEFAULT 0 CHECK (reorder_level_milli >= 0),
    active               boolean NOT NULL DEFAULT true,
    created_at           timestamptz NOT NULL DEFAULT now(),
    updated_at           timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id),
    UNIQUE (establishment_id, sku),
    CHECK (kind = 'good' OR NOT track_stock),
    FOREIGN KEY (establishment_id, category_id) REFERENCES product_categories (establishment_id, id),
    FOREIGN KEY (establishment_id, tax_rate_id) REFERENCES tax_rates (establishment_id, id)
);
CREATE UNIQUE INDEX products_barcode ON products (establishment_id, barcode) WHERE barcode IS NOT NULL;
CREATE INDEX products_name ON products (establishment_id, lower(name));
CREATE TRIGGER products_touch BEFORE UPDATE ON products FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

CREATE TABLE warehouses (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    name             text NOT NULL,
    is_default       boolean NOT NULL DEFAULT false,
    active           boolean NOT NULL DEFAULT true,
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id)
);
CREATE UNIQUE INDEX warehouses_one_default ON warehouses (establishment_id) WHERE is_default;

CREATE TABLE stock_levels (
    establishment_id uuid NOT NULL,
    product_id       uuid NOT NULL,
    warehouse_id     uuid NOT NULL,
    quantity_milli   bigint NOT NULL DEFAULT 0,
    updated_at       timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (establishment_id, product_id, warehouse_id),
    FOREIGN KEY (establishment_id, product_id) REFERENCES products (establishment_id, id),
    FOREIGN KEY (establishment_id, warehouse_id) REFERENCES warehouses (establishment_id, id)
);

CREATE TABLE stock_movements (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    establishment_id uuid NOT NULL,
    product_id       uuid NOT NULL,
    warehouse_id     uuid NOT NULL,
    quantity_milli   bigint NOT NULL CHECK (quantity_milli <> 0),
    kind             text NOT NULL CHECK (kind IN ('opening', 'purchase', 'sale', 'return_in', 'return_out',
                                                   'adjustment', 'transfer_in', 'transfer_out')),
    source_type      text,
    source_id        uuid,
    note             text NOT NULL DEFAULT '',
    created_by       uuid,
    created_at       timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, product_id) REFERENCES products (establishment_id, id),
    FOREIGN KEY (establishment_id, warehouse_id) REFERENCES warehouses (establishment_id, id)
);
CREATE INDEX stock_movements_product ON stock_movements (establishment_id, product_id, created_at DESC);
CREATE INDEX stock_movements_date ON stock_movements (establishment_id, created_at DESC);

-- Stock levels change ONLY through movements. The application role has no
-- write privilege on stock_levels; this definer function maintains it.
CREATE FUNCTION nh_apply_stock_movement() RETURNS trigger
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    v_track   boolean;
    v_new     bigint;
    v_allow   boolean;
BEGIN
    SELECT track_stock INTO v_track FROM products
     WHERE id = NEW.product_id AND establishment_id = NEW.establishment_id;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'unknown product' USING ERRCODE = 'NH003';
    END IF;
    IF NOT v_track THEN
        RAISE EXCEPTION 'product does not track stock' USING ERRCODE = 'NH004';
    END IF;

    INSERT INTO stock_levels (establishment_id, product_id, warehouse_id, quantity_milli, updated_at)
    VALUES (NEW.establishment_id, NEW.product_id, NEW.warehouse_id, NEW.quantity_milli, now())
    ON CONFLICT (establishment_id, product_id, warehouse_id)
    DO UPDATE SET quantity_milli = stock_levels.quantity_milli + EXCLUDED.quantity_milli,
                  updated_at = now()
    RETURNING quantity_milli INTO v_new;

    IF v_new < 0 AND NEW.quantity_milli < 0 THEN
        SELECT allow_negative_stock INTO v_allow FROM establishment_settings
         WHERE establishment_id = NEW.establishment_id;
        IF NOT COALESCE(v_allow, false) THEN
            RAISE EXCEPTION 'insufficient stock for product %', NEW.product_id USING ERRCODE = 'NH001';
        END IF;
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER stock_movements_apply AFTER INSERT ON stock_movements
    FOR EACH ROW EXECUTE FUNCTION nh_apply_stock_movement();

-- ---------------------------------------------------------------- parties
CREATE TABLE parties (
    id                 uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id   uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    is_customer        boolean NOT NULL DEFAULT false,
    is_supplier        boolean NOT NULL DEFAULT false,
    kind               text NOT NULL DEFAULT 'company' CHECK (kind IN ('individual', 'company')),
    name               text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 200),
    email              text,
    phone              text,
    address            text NOT NULL DEFAULT '',
    city               text NOT NULL DEFAULT '',
    country_code       char(2),
    tax_ids            jsonb NOT NULL DEFAULT '{}'::jsonb,
    payment_terms_days integer NOT NULL DEFAULT 0 CHECK (payment_terms_days BETWEEN 0 AND 365),
    notes              text NOT NULL DEFAULT '',
    active             boolean NOT NULL DEFAULT true,
    created_at         timestamptz NOT NULL DEFAULT now(),
    updated_at         timestamptz NOT NULL DEFAULT now(),
    CHECK (is_customer OR is_supplier),
    UNIQUE (establishment_id, id)
);
CREATE INDEX parties_name ON parties (establishment_id, lower(name));
CREATE TRIGGER parties_touch BEFORE UPDATE ON parties FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

-- ---------------------------------------------------------------- sales documents
CREATE TABLE sales_documents (
    id                  uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id    uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    doc_type            text NOT NULL CHECK (doc_type IN ('quote', 'order', 'invoice', 'credit_note')),
    status              text NOT NULL DEFAULT 'draft',
    number              text,
    sequence_value      bigint,
    party_id            uuid NOT NULL,
    currency            char(3) NOT NULL REFERENCES currencies(code),
    language            text NOT NULL DEFAULT 'fr' CHECK (language IN ('fr', 'en')),
    issue_date          date NOT NULL DEFAULT current_date,
    due_date            date,
    valid_until         date,
    main_currency       char(3) NOT NULL REFERENCES currencies(code),
    fx_rate             numeric(24, 10) CHECK (fx_rate IS NULL OR fx_rate > 0),
    fx_rate_date        date,
    fx_rate_source      text,
    tax_rounding        text NOT NULL DEFAULT 'per_line' CHECK (tax_rounding IN ('per_line', 'per_rate')),
    gross_minor         bigint NOT NULL DEFAULT 0,
    discount_minor      bigint NOT NULL DEFAULT 0,
    net_minor           bigint NOT NULL DEFAULT 0,
    tax_minor           bigint NOT NULL DEFAULT 0,
    total_minor         bigint NOT NULL DEFAULT 0 CHECK (total_minor >= 0),
    paid_minor          bigint NOT NULL DEFAULT 0 CHECK (paid_minor >= 0),
    credited_minor      bigint NOT NULL DEFAULT 0 CHECK (credited_minor >= 0),
    tax_breakdown       jsonb NOT NULL DEFAULT '[]'::jsonb,
    notes               text NOT NULL DEFAULT '',
    terms               text NOT NULL DEFAULT '',
    related_document_id uuid,
    restock             boolean NOT NULL DEFAULT false,      -- credit notes: return goods to stock
    seller_snapshot     jsonb,
    buyer_snapshot      jsonb,
    profile_code        text,
    profile_version     integer,
    fiscal_reference    jsonb,                               -- DEF reference recorded by the user (CD)
    pdf_sha256          text,
    created_by          uuid,
    issued_by           uuid,
    issued_at           timestamptz,
    created_at          timestamptz NOT NULL DEFAULT now(),
    updated_at          timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id),
    UNIQUE (establishment_id, doc_type, number),
    CHECK ((status = 'draft') = (number IS NULL)),
    -- A credited invoice that had been paid leaves a negative balance (amount to refund).
    CHECK (paid_minor <= total_minor AND credited_minor <= total_minor),
    CHECK (
        (doc_type = 'quote'       AND status IN ('draft', 'sent', 'accepted', 'declined', 'converted')) OR
        (doc_type = 'order'       AND status IN ('draft', 'confirmed', 'converted', 'canceled')) OR
        (doc_type = 'invoice'     AND status IN ('draft', 'issued', 'partially_paid', 'paid', 'credited')) OR
        (doc_type = 'credit_note' AND status IN ('draft', 'issued'))
    ),
    FOREIGN KEY (establishment_id, party_id) REFERENCES parties (establishment_id, id),
    FOREIGN KEY (establishment_id, related_document_id) REFERENCES sales_documents (establishment_id, id)
);
CREATE INDEX sales_documents_list ON sales_documents (establishment_id, doc_type, issue_date DESC);
CREATE INDEX sales_documents_party ON sales_documents (establishment_id, party_id);
CREATE INDEX sales_documents_open ON sales_documents (establishment_id, due_date)
    WHERE doc_type = 'invoice' AND status IN ('issued', 'partially_paid');

CREATE TABLE sales_document_lines (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL,
    document_id      uuid NOT NULL,
    line_no          integer NOT NULL CHECK (line_no > 0),
    product_id       uuid,
    description      text NOT NULL CHECK (length(btrim(description)) BETWEEN 1 AND 500),
    quantity_milli   bigint NOT NULL CHECK (quantity_milli > 0),
    unit             text NOT NULL DEFAULT 'unit',
    unit_price_minor bigint NOT NULL CHECK (unit_price_minor >= 0),
    discount_bp      integer NOT NULL DEFAULT 0 CHECK (discount_bp BETWEEN 0 AND 10000),
    tax_rate_bp      integer NOT NULL DEFAULT 0 CHECK (tax_rate_bp BETWEEN 0 AND 10000),
    tax_code         text NOT NULL DEFAULT '',
    gross_minor      bigint NOT NULL,
    discount_minor   bigint NOT NULL,
    net_minor        bigint NOT NULL,
    tax_minor        bigint NOT NULL,
    total_minor      bigint NOT NULL,
    UNIQUE (document_id, line_no),
    FOREIGN KEY (establishment_id, document_id) REFERENCES sales_documents (establishment_id, id) ON DELETE CASCADE,
    FOREIGN KEY (establishment_id, product_id) REFERENCES products (establishment_id, id)
);

-- Issued documents are immutable; only status/payment fields may evolve.
CREATE FUNCTION nh_guard_sales_document() RETURNS trigger
LANGUAGE plpgsql AS $$
DECLARE
    mutable text[] := ARRAY['status', 'paid_minor', 'credited_minor', 'pdf_sha256', 'fiscal_reference', 'updated_at'];
BEGIN
    IF TG_OP = 'DELETE' THEN
        IF OLD.status <> 'draft' THEN
            RAISE EXCEPTION 'issued document % cannot be deleted', OLD.number USING ERRCODE = 'NH002';
        END IF;
        RETURN OLD;
    END IF;
    NEW.updated_at := now();
    IF OLD.status <> 'draft' THEN
        IF NEW.status = 'draft' THEN
            RAISE EXCEPTION 'document % cannot return to draft', OLD.number USING ERRCODE = 'NH002';
        END IF;
        IF (to_jsonb(NEW) - mutable) IS DISTINCT FROM (to_jsonb(OLD) - mutable) THEN
            RAISE EXCEPTION 'issued document % is immutable', OLD.number USING ERRCODE = 'NH002';
        END IF;
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER sales_documents_guard BEFORE UPDATE OR DELETE ON sales_documents
    FOR EACH ROW EXECUTE FUNCTION nh_guard_sales_document();

CREATE FUNCTION nh_guard_sales_line() RETURNS trigger
LANGUAGE plpgsql AS $$
DECLARE
    v_status text;
    v_doc    uuid := COALESCE(NEW.document_id, OLD.document_id);
BEGIN
    SELECT status INTO v_status FROM sales_documents WHERE id = v_doc;
    IF FOUND AND v_status <> 'draft' THEN
        RAISE EXCEPTION 'lines of an issued document are immutable' USING ERRCODE = 'NH002';
    END IF;
    IF TG_OP = 'DELETE' THEN RETURN OLD; END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER sales_document_lines_guard BEFORE INSERT OR UPDATE OR DELETE ON sales_document_lines
    FOR EACH ROW EXECUTE FUNCTION nh_guard_sales_line();

CREATE TABLE document_sequences (
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    doc_type         text NOT NULL,
    period_key       text NOT NULL,
    next_value       bigint NOT NULL CHECK (next_value > 0),
    PRIMARY KEY (establishment_id, doc_type, period_key)
);

CREATE TABLE document_status_history (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    establishment_id uuid NOT NULL,
    document_id      uuid NOT NULL,
    from_status      text,
    to_status        text NOT NULL,
    actor_id         uuid,
    note             text NOT NULL DEFAULT '',
    created_at       timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, document_id) REFERENCES sales_documents (establishment_id, id) ON DELETE CASCADE
);
CREATE INDEX document_status_history_doc ON document_status_history (document_id, created_at);

-- ---------------------------------------------------------------- purchases
CREATE TABLE purchases (
    id                 uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id   uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    supplier_id        uuid NOT NULL,
    status             text NOT NULL DEFAULT 'draft' CHECK (status IN ('draft', 'received', 'canceled')),
    supplier_reference text NOT NULL DEFAULT '',
    purchase_date      date NOT NULL DEFAULT current_date,
    due_date           date,
    currency           char(3) NOT NULL REFERENCES currencies(code),
    warehouse_id       uuid NOT NULL,
    net_minor          bigint NOT NULL DEFAULT 0,
    tax_minor          bigint NOT NULL DEFAULT 0,
    total_minor        bigint NOT NULL DEFAULT 0 CHECK (total_minor >= 0),
    paid_minor         bigint NOT NULL DEFAULT 0 CHECK (paid_minor >= 0),
    notes              text NOT NULL DEFAULT '',
    created_by         uuid,
    received_by        uuid,
    received_at        timestamptz,
    created_at         timestamptz NOT NULL DEFAULT now(),
    updated_at         timestamptz NOT NULL DEFAULT now(),
    CHECK (paid_minor <= total_minor),
    UNIQUE (establishment_id, id),
    FOREIGN KEY (establishment_id, supplier_id) REFERENCES parties (establishment_id, id),
    FOREIGN KEY (establishment_id, warehouse_id) REFERENCES warehouses (establishment_id, id)
);
CREATE INDEX purchases_list ON purchases (establishment_id, purchase_date DESC);

CREATE TABLE purchase_lines (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL,
    purchase_id      uuid NOT NULL,
    line_no          integer NOT NULL CHECK (line_no > 0),
    product_id       uuid,
    description      text NOT NULL,
    quantity_milli   bigint NOT NULL CHECK (quantity_milli > 0),
    unit_cost_minor  bigint NOT NULL CHECK (unit_cost_minor >= 0),
    tax_rate_bp      integer NOT NULL DEFAULT 0 CHECK (tax_rate_bp BETWEEN 0 AND 10000),
    net_minor        bigint NOT NULL,
    tax_minor        bigint NOT NULL,
    total_minor      bigint NOT NULL,
    UNIQUE (purchase_id, line_no),
    FOREIGN KEY (establishment_id, purchase_id) REFERENCES purchases (establishment_id, id) ON DELETE CASCADE,
    FOREIGN KEY (establishment_id, product_id) REFERENCES products (establishment_id, id)
);

CREATE FUNCTION nh_guard_purchase() RETURNS trigger
LANGUAGE plpgsql AS $$
DECLARE
    mutable text[] := ARRAY['status', 'paid_minor', 'updated_at'];
BEGIN
    IF TG_OP = 'DELETE' THEN
        IF OLD.status = 'received' THEN
            RAISE EXCEPTION 'received purchase cannot be deleted' USING ERRCODE = 'NH002';
        END IF;
        RETURN OLD;
    END IF;
    NEW.updated_at := now();
    IF OLD.status = 'received' THEN
        IF NEW.status <> 'received' OR (to_jsonb(NEW) - mutable) IS DISTINCT FROM (to_jsonb(OLD) - mutable) THEN
            RAISE EXCEPTION 'received purchase is immutable' USING ERRCODE = 'NH002';
        END IF;
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER purchases_guard BEFORE UPDATE OR DELETE ON purchases
    FOR EACH ROW EXECUTE FUNCTION nh_guard_purchase();

-- ---------------------------------------------------------------- cash & payments
CREATE TABLE cash_accounts (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    name             text NOT NULL,
    kind             text NOT NULL CHECK (kind IN ('cash', 'bank', 'mobile_money', 'other')),
    currency         char(3) NOT NULL REFERENCES currencies(code),
    active           boolean NOT NULL DEFAULT true,
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id)
);

CREATE TABLE expense_categories (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    name             text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 80),
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, id)
);
CREATE UNIQUE INDEX expense_categories_name ON expense_categories (establishment_id, lower(name));

CREATE TABLE expenses (
    id                uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id  uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    category_id       uuid,
    supplier_id       uuid,
    description       text NOT NULL CHECK (length(btrim(description)) BETWEEN 1 AND 300),
    amount_minor      bigint NOT NULL CHECK (amount_minor > 0),      -- tax included
    tax_minor         bigint NOT NULL DEFAULT 0 CHECK (tax_minor >= 0),
    currency          char(3) NOT NULL REFERENCES currencies(code),
    expense_date      date NOT NULL DEFAULT current_date,
    due_date          date,
    paid_minor        bigint NOT NULL DEFAULT 0 CHECK (paid_minor >= 0),
    receipt_object_id uuid,
    created_by        uuid,
    created_at        timestamptz NOT NULL DEFAULT now(),
    updated_at        timestamptz NOT NULL DEFAULT now(),
    CHECK (tax_minor <= amount_minor),
    CHECK (paid_minor <= amount_minor),
    UNIQUE (establishment_id, id),
    FOREIGN KEY (establishment_id, category_id) REFERENCES expense_categories (establishment_id, id),
    FOREIGN KEY (establishment_id, supplier_id) REFERENCES parties (establishment_id, id),
    FOREIGN KEY (establishment_id, receipt_object_id) REFERENCES objects (establishment_id, id)
);
CREATE INDEX expenses_date ON expenses (establishment_id, expense_date DESC);
CREATE TRIGGER expenses_touch BEFORE UPDATE ON expenses FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

CREATE TABLE payments (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    direction        text NOT NULL CHECK (direction IN ('in', 'out')),
    party_id         uuid,
    document_id      uuid,
    purchase_id      uuid,
    expense_id       uuid,
    method           text NOT NULL CHECK (method IN ('cash', 'mobile_money', 'card', 'bank_transfer', 'cheque', 'other')),
    account_id       uuid,
    amount_minor     bigint NOT NULL CHECK (amount_minor > 0),
    currency         char(3) NOT NULL REFERENCES currencies(code),
    paid_at          timestamptz NOT NULL DEFAULT now(),
    reference        text NOT NULL DEFAULT '',
    note             text NOT NULL DEFAULT '',
    created_by       uuid,
    created_at       timestamptz NOT NULL DEFAULT now(),
    CHECK (num_nonnulls(document_id, purchase_id, expense_id) <= 1),
    UNIQUE (establishment_id, id),
    FOREIGN KEY (establishment_id, party_id) REFERENCES parties (establishment_id, id),
    FOREIGN KEY (establishment_id, document_id) REFERENCES sales_documents (establishment_id, id),
    FOREIGN KEY (establishment_id, purchase_id) REFERENCES purchases (establishment_id, id),
    FOREIGN KEY (establishment_id, expense_id) REFERENCES expenses (establishment_id, id),
    FOREIGN KEY (establishment_id, account_id) REFERENCES cash_accounts (establishment_id, id)
);
CREATE INDEX payments_date ON payments (establishment_id, paid_at DESC);

CREATE TABLE exchange_rates (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    from_currency    char(3) NOT NULL REFERENCES currencies(code),
    to_currency      char(3) NOT NULL REFERENCES currencies(code),
    rate             numeric(24, 10) NOT NULL CHECK (rate > 0),
    rate_date        date NOT NULL,
    source           text NOT NULL DEFAULT '',
    created_by       uuid,
    created_at       timestamptz NOT NULL DEFAULT now(),
    CHECK (from_currency <> to_currency),
    UNIQUE (establishment_id, from_currency, to_currency, rate_date)
);

-- Request idempotency, stored in the same transaction as the operation.
CREATE TABLE idempotency_keys (
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    idem_key         text NOT NULL CHECK (length(idem_key) BETWEEN 8 AND 100),
    user_id          uuid,
    request_hash     bytea NOT NULL,
    status_code      integer,
    response_body    text,
    created_at       timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (establishment_id, idem_key)
);

-- ---------------------------------------------------------------- usage counters (platform)
-- Aggregates visible to the platform administrators. They contain counts only,
-- never business content, and are fed by definer triggers.
CREATE TABLE usage_daily (
    day                date NOT NULL,
    establishment_id   uuid NOT NULL,
    documents_issued   integer NOT NULL DEFAULT 0,
    payments_recorded  integer NOT NULL DEFAULT 0,
    PRIMARY KEY (day, establishment_id)
);

CREATE FUNCTION nh_count_document_issue() RETURNS trigger
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
BEGIN
    IF OLD.status = 'draft' AND NEW.status <> 'draft' THEN
        INSERT INTO usage_daily (day, establishment_id, documents_issued) VALUES (current_date, NEW.establishment_id, 1)
        ON CONFLICT (day, establishment_id) DO UPDATE SET documents_issued = usage_daily.documents_issued + 1;
    END IF;
    RETURN NULL;
END $$;
CREATE TRIGGER sales_documents_usage AFTER UPDATE ON sales_documents
    FOR EACH ROW EXECUTE FUNCTION nh_count_document_issue();

CREATE FUNCTION nh_count_payment() RETURNS trigger
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
BEGIN
    INSERT INTO usage_daily (day, establishment_id, payments_recorded) VALUES (current_date, NEW.establishment_id, 1)
    ON CONFLICT (day, establishment_id) DO UPDATE SET payments_recorded = usage_daily.payments_recorded + 1;
    RETURN NULL;
END $$;
CREATE TRIGGER payments_usage AFTER INSERT ON payments
    FOR EACH ROW EXECUTE FUNCTION nh_count_payment();
