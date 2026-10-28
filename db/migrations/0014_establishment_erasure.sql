-- Nhill migration 0014 — permanent erasure of an establishment, retention clean-ups.
--
-- The terms of service promise: at the end of the contract, the data stays readable and
-- exportable for 90 days, then is deleted. Until now nothing could delete it: issued
-- documents, received purchases and the audit trail are immutable for the application.
--
-- 1. nh_establishment_erasure(id): when an establishment may be erased (90 days after
--    the end of its full access; never while it has full access or is suspended).
-- 2. nh_erasing(id): true only inside nh_erase_establishment (a transaction setting AND
--    the current role is the owner of the schema objects). The guard triggers of the
--    immutable tables let the erasure through and nothing else: the application role
--    can set the setting but can never be the owner.
-- 3. nh_erase_establishment(...): archives the paid subscription orders (accounting of
--    Nhill Solution, 10 years), deletes the establishment with everything attached to it
--    (cascades), its audit trail and subscription history, then the user accounts that
--    belonged to it and to no other establishment.
-- 4. Retention: the application may delete sent e-mails and expired sessions (job).

-- ---------------------------------------------------------------- 1. billing archive
CREATE TABLE billing_archive (
    id                 bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    establishment_id   uuid NOT NULL,                -- no foreign key: the establishment is gone
    trade_name         text NOT NULL,
    legal_name         text,
    country_code       char(2),
    order_id           uuid NOT NULL UNIQUE,
    plan_code          text,
    kind               text,
    duration_months    integer,
    subtotal_minor     bigint,
    discount_minor     bigint,
    tax_minor          bigint,
    amount_minor       bigint NOT NULL,
    currency           char(3) NOT NULL,
    status             text NOT NULL,
    provider           text,
    provider_reference text,
    created_at         timestamptz NOT NULL,
    paid_at            timestamptz,
    archived_at        timestamptz NOT NULL DEFAULT now(),
    keep_until         date NOT NULL                 -- accounting records: 10 years
);
CREATE INDEX billing_archive_est ON billing_archive (establishment_id);
GRANT SELECT ON billing_archive TO nhill_app;

-- ---------------------------------------------------------------- 2. eligibility
-- access_ended_at: last moment of full access (end of the grace period, cancellation,
-- closure); erasable_from = access_ended_at + 90 days.
CREATE FUNCTION nh_establishment_erasure(p_est uuid)
RETURNS TABLE (eligible boolean, reason text, access_ended_at timestamptz, erasable_from timestamptz)
LANGUAGE plpgsql STABLE SET search_path = nhill, pg_catalog AS $$
DECLARE
    a         record;
    v_created timestamptz;
    v_updated timestamptz;
    ended     timestamptz;
BEGIN
    SELECT e.created_at, e.updated_at INTO v_created, v_updated FROM establishments e WHERE e.id = p_est;
    IF NOT FOUND THEN
        RETURN;
    END IF;
    SELECT * INTO a FROM nh_subscription_access(p_est);
    IF a.access = 'full' THEN
        RETURN QUERY SELECT false, 'access_active'::text, NULL::timestamptz, NULL::timestamptz;
        RETURN;
    END IF;
    IF a.reason = 'establishment_suspended' OR a.status = 'suspended' THEN
        RETURN QUERY SELECT false, 'suspended'::text, NULL::timestamptz, NULL::timestamptz;
        RETURN;
    END IF;
    ended := CASE a.reason
                 WHEN 'expired' THEN a.grace_end
                 WHEN 'canceled' THEN COALESCE(
                     (SELECT s.canceled_at FROM subscriptions s WHERE s.establishment_id = p_est),
                     (SELECT max(ev.created_at) FROM subscription_events ev
                       WHERE ev.establishment_id = p_est AND ev.to_status = 'canceled'))
                 WHEN 'establishment_closed' THEN v_updated
             END;
    ended := COALESCE(ended,
                      (SELECT max(ev.created_at) FROM subscription_events ev WHERE ev.establishment_id = p_est),
                      v_created);
    RETURN QUERY SELECT now() >= ended + interval '90 days', a.reason, ended, ended + interval '90 days';
END $$;
GRANT EXECUTE ON FUNCTION nh_establishment_erasure(uuid) TO nhill_app;

-- ---------------------------------------------------------------- 3. erasure flag
CREATE FUNCTION nh_erasing(p_est uuid) RETURNS boolean
LANGUAGE sql STABLE SET search_path = nhill, pg_catalog AS $$
    SELECT p_est IS NOT NULL
       AND COALESCE(current_setting('nhill.erasing_establishment', true), '') = p_est::text
       AND current_user = (SELECT pg_get_userbyid(c.relowner) FROM pg_class c
                            WHERE c.oid = 'nhill.establishments'::regclass)
$$;

-- Guard triggers of 0002, unchanged except for the erasure.
CREATE OR REPLACE FUNCTION nh_guard_sales_document() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
DECLARE
    mutable text[] := ARRAY['status', 'paid_minor', 'credited_minor', 'pdf_sha256', 'fiscal_reference', 'updated_at'];
BEGIN
    IF TG_OP = 'DELETE' THEN
        IF OLD.status <> 'draft' AND NOT nh_erasing(OLD.establishment_id) THEN
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

CREATE OR REPLACE FUNCTION nh_guard_sales_line() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
DECLARE
    v_status text;
    v_doc    uuid := COALESCE(NEW.document_id, OLD.document_id);
BEGIN
    IF TG_OP = 'DELETE' AND nh_erasing(OLD.establishment_id) THEN
        RETURN OLD;
    END IF;
    SELECT status INTO v_status FROM sales_documents WHERE id = v_doc;
    IF FOUND AND v_status <> 'draft' THEN
        RAISE EXCEPTION 'lines of an issued document are immutable' USING ERRCODE = 'NH002';
    END IF;
    IF TG_OP = 'DELETE' THEN RETURN OLD; END IF;
    RETURN NEW;
END $$;

CREATE OR REPLACE FUNCTION nh_guard_purchase() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
DECLARE
    mutable text[] := ARRAY['status', 'paid_minor', 'updated_at'];
BEGIN
    IF TG_OP = 'DELETE' THEN
        IF OLD.status = 'received' AND NOT nh_erasing(OLD.establishment_id) THEN
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

-- ---------------------------------------------------------------- 4. erasure
-- p_early: erasure at the customer's request before the 90 days (never while the
-- establishment has full access). Returns counts only (no content).
CREATE FUNCTION nh_erase_establishment(p_est uuid, p_early boolean)
RETURNS jsonb
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    e         record;
    elig      record;
    v_users   uuid[];
    v_emails  text[];
    n_docs    bigint;
    n_parties bigint;
    n_archived bigint;
    n_users   bigint;
    n_audit   bigint;
    v_tables  text[];
    v_left    text[] := '{}';
    v_table   text;
    pass      integer;
BEGIN
    SELECT * INTO e FROM establishments WHERE id = p_est FOR UPDATE;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'unknown establishment' USING ERRCODE = 'NH010';
    END IF;
    SELECT * INTO elig FROM nh_establishment_erasure(p_est);
    IF NOT elig.eligible AND NOT (p_early AND elig.reason NOT IN ('access_active')) THEN
        RAISE EXCEPTION 'establishment cannot be erased yet (%)', elig.reason USING ERRCODE = 'NH011';
    END IF;

    PERFORM set_config('app.establishment_id', p_est::text, true);
    PERFORM set_config('nhill.erasing_establishment', p_est::text, true);

    SELECT count(*) INTO n_docs FROM sales_documents WHERE establishment_id = p_est;
    SELECT count(*) INTO n_parties FROM parties WHERE establishment_id = p_est;

    INSERT INTO billing_archive (establishment_id, trade_name, legal_name, country_code, order_id, plan_code, kind,
                                 duration_months, subtotal_minor, discount_minor, tax_minor, amount_minor, currency,
                                 status, provider, provider_reference, created_at, paid_at, keep_until)
    SELECT p_est, e.trade_name, e.legal_name, e.country_code, o.id, p.code, o.kind, o.duration_months, o.subtotal_minor,
           o.discount_minor, o.tax_minor, o.amount_minor, o.currency, o.status,
           o.provider, o.provider_reference, o.created_at, o.paid_at,
           (COALESCE(o.paid_at, o.created_at) + interval '10 years')::date
      FROM subscription_orders o LEFT JOIN plans p ON p.id = o.plan_id
     WHERE o.establishment_id = p_est AND o.status IN ('paid', 'refunded');
    GET DIAGNOSTICS n_archived = ROW_COUNT;

    SELECT COALESCE(array_agg(DISTINCT m.user_id), '{}') INTO v_users FROM memberships m WHERE m.establishment_id = p_est;

    DELETE FROM attachment_requests WHERE establishment_id = p_est;
    DELETE FROM subscription_events WHERE establishment_id = p_est;
    DELETE FROM audit_log WHERE establishment_id = p_est;
    GET DIAGNOSTICS n_audit = ROW_COUNT;
    UPDATE trial_registry SET establishment_id = NULL WHERE establishment_id = p_est;

    -- Every table holding the establishment's rows, children first. Foreign keys between
    -- business tables are NO ACTION (checked at the end of each cascaded statement), so a
    -- single DELETE on establishments cannot remove, say, a product still referenced by an
    -- invoice line. Each pass deletes what no remaining row references; the order is found
    -- by the database itself and stays right when tables are added later.
    SELECT array_agg(cl.relname::text) INTO v_tables
      FROM pg_attribute at
      JOIN pg_class cl ON cl.oid = at.attrelid
      JOIN pg_namespace ns ON ns.oid = cl.relnamespace
     WHERE ns.nspname = 'nhill' AND cl.relkind IN ('r', 'p') AND at.attname = 'establishment_id' AND NOT at.attisdropped
       AND cl.relname NOT IN ('establishments', 'audit_log', 'subscription_events', 'trial_registry', 'billing_archive',
                              'error_events');
    FOR pass IN 1..30 LOOP
        v_left := '{}';
        FOREACH v_table IN ARRAY v_tables LOOP
            BEGIN
                EXECUTE format('DELETE FROM nhill.%I WHERE establishment_id = $1', v_table) USING p_est;
            EXCEPTION WHEN foreign_key_violation THEN
                v_left := v_left || v_table;
            END;
        END LOOP;
        EXIT WHEN cardinality(v_left) = 0;
        IF cardinality(v_left) = cardinality(v_tables) THEN
            RAISE EXCEPTION 'erasure blocked by references between %', v_left USING ERRCODE = '23503';
        END IF;
        v_tables := v_left;
    END LOOP;
    IF cardinality(v_left) > 0 THEN
        RAISE EXCEPTION 'erasure blocked by references between %', v_left USING ERRCODE = '23503';
    END IF;
    DELETE FROM establishments WHERE id = p_est;

    -- Accounts that belonged only to this establishment (no membership left anywhere).
    SELECT COALESCE(array_agg(u.email), '{}') INTO v_emails FROM users u
     WHERE u.id = ANY (v_users) AND NOT EXISTS (SELECT 1 FROM memberships m WHERE m.user_id = u.id);
    DELETE FROM email_outbox WHERE to_address = ANY (v_emails) OR params ->> 'establishment_id' = p_est::text;
    DELETE FROM users u WHERE u.id = ANY (v_users) AND NOT EXISTS (SELECT 1 FROM memberships m WHERE m.user_id = u.id);
    GET DIAGNOSTICS n_users = ROW_COUNT;

    PERFORM set_config('nhill.erasing_establishment', '', true);
    RETURN jsonb_build_object('documents', n_docs, 'parties', n_parties, 'users_deleted', n_users,
                              'audit_entries_deleted', n_audit, 'billing_orders_archived', n_archived,
                              'access_reason', elig.reason, 'early', p_early AND NOT elig.eligible);
END $$;
REVOKE ALL ON FUNCTION nh_erase_establishment(uuid, boolean) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION nh_erase_establishment(uuid, boolean) TO nhill_app;

-- ---------------------------------------------------------------- 5. retention clean-ups
-- Sent or failed e-mails (addresses, parameters) and sessions that ended long ago.
GRANT DELETE ON email_outbox, sessions TO nhill_app;
