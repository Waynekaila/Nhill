-- Nhill migration 0006 — activity journal per employee and platform statistics.
--
-- 1. Faster activity queries per employee (audit_log by actor).
-- 2. Platform statistics for the Nhill operator. Business tables stay
--    protected by row-level security with no bypass: the operator can only call
--    the SECURITY DEFINER functions below, which return AGGREGATES (counts and
--    sums per currency) for one establishment at a time — never customers,
--    products, document lines or any individual record.

CREATE INDEX audit_log_actor ON audit_log (establishment_id, actor_id, created_at DESC);

-- Volumes of one establishment over a period, per currency.
CREATE FUNCTION nh_platform_establishment_volumes(p_est uuid, p_from date, p_to date)
RETURNS TABLE (
    currency             text,
    invoices_issued      bigint,
    invoiced_net_minor   bigint,
    invoiced_total_minor bigint,
    credit_notes_issued  bigint,
    credited_total_minor bigint,
    payments_in_count    bigint,
    payments_in_minor    bigint,
    payments_out_minor   bigint,
    expenses_count       bigint,
    expenses_minor       bigint,
    purchases_minor      bigint
)
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    previous text := current_setting('app.establishment_id', true);
BEGIN
    PERFORM set_config('app.establishment_id', p_est::text, true);
    RETURN QUERY
    WITH d AS (
        SELECT s.currency AS cur,
               count(*) FILTER (WHERE s.doc_type = 'invoice') AS inv,
               COALESCE(sum(s.net_minor) FILTER (WHERE s.doc_type = 'invoice'), 0) AS inv_net,
               COALESCE(sum(s.total_minor) FILTER (WHERE s.doc_type = 'invoice'), 0) AS inv_total,
               count(*) FILTER (WHERE s.doc_type = 'credit_note') AS cn,
               COALESCE(sum(s.total_minor) FILTER (WHERE s.doc_type = 'credit_note'), 0) AS cn_total
        FROM sales_documents s
        WHERE s.status NOT IN ('draft', 'canceled') AND s.doc_type IN ('invoice', 'credit_note')
          AND s.issue_date BETWEEN p_from AND p_to
        GROUP BY s.currency),
    y AS (
        SELECT p.currency AS cur,
               count(*) FILTER (WHERE p.direction = 'in') AS pin_n,
               COALESCE(sum(p.amount_minor) FILTER (WHERE p.direction = 'in'), 0) AS pin,
               COALESCE(sum(p.amount_minor) FILTER (WHERE p.direction = 'out'), 0) AS pout
        FROM payments p
        WHERE p.paid_at >= nh_local_start(p_from) AND p.paid_at < nh_local_start(p_to + 1)
        GROUP BY p.currency),
    e AS (
        SELECT x.currency AS cur, count(*) AS n, COALESCE(sum(x.amount_minor), 0) AS total
        FROM expenses x WHERE x.expense_date BETWEEN p_from AND p_to GROUP BY x.currency),
    pu AS (
        SELECT x.currency AS cur, COALESCE(sum(x.total_minor), 0) AS total
        FROM purchases x WHERE x.status = 'received' AND x.purchase_date BETWEEN p_from AND p_to GROUP BY x.currency),
    curs AS (SELECT cur FROM d UNION SELECT cur FROM y UNION SELECT cur FROM e UNION SELECT cur FROM pu)
    SELECT c.cur::text,
           COALESCE(d.inv, 0)::bigint, COALESCE(d.inv_net, 0)::bigint, COALESCE(d.inv_total, 0)::bigint,
           COALESCE(d.cn, 0)::bigint, COALESCE(d.cn_total, 0)::bigint,
           COALESCE(y.pin_n, 0)::bigint, COALESCE(y.pin, 0)::bigint, COALESCE(y.pout, 0)::bigint,
           COALESCE(e.n, 0)::bigint, COALESCE(e.total, 0)::bigint, COALESCE(pu.total, 0)::bigint
    FROM curs c
    LEFT JOIN d ON d.cur = c.cur LEFT JOIN y ON y.cur = c.cur
    LEFT JOIN e ON e.cur = c.cur LEFT JOIN pu ON pu.cur = c.cur
    ORDER BY c.cur;
    PERFORM set_config('app.establishment_id', COALESCE(previous, ''), true);
END $$;

-- Monthly series of one establishment (issued invoices / credit notes / payments received).
CREATE FUNCTION nh_platform_establishment_monthly(p_est uuid, p_from date, p_to date)
RETURNS TABLE (
    month                text,
    currency             text,
    invoices_issued      bigint,
    invoiced_net_minor   bigint,
    invoiced_total_minor bigint,
    credited_total_minor bigint,
    payments_in_minor    bigint,
    expenses_minor       bigint
)
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    previous text := current_setting('app.establishment_id', true);
BEGIN
    PERFORM set_config('app.establishment_id', p_est::text, true);
    RETURN QUERY
    WITH d AS (
        SELECT to_char(s.issue_date, 'YYYY-MM') AS m, s.currency AS cur,
               count(*) FILTER (WHERE s.doc_type = 'invoice') AS inv,
               COALESCE(sum(s.net_minor) FILTER (WHERE s.doc_type = 'invoice'), 0) AS net,
               COALESCE(sum(s.total_minor) FILTER (WHERE s.doc_type = 'invoice'), 0) AS total,
               COALESCE(sum(s.total_minor) FILTER (WHERE s.doc_type = 'credit_note'), 0) AS cn
        FROM sales_documents s
        WHERE s.status NOT IN ('draft', 'canceled') AND s.doc_type IN ('invoice', 'credit_note')
          AND s.issue_date BETWEEN p_from AND p_to
        GROUP BY 1, 2),
    y AS (
        SELECT to_char(nh_local_date(p.paid_at), 'YYYY-MM') AS m, p.currency AS cur, sum(p.amount_minor) AS pin
        FROM payments p WHERE p.direction = 'in' AND p.paid_at >= nh_local_start(p_from) AND p.paid_at < nh_local_start(p_to + 1) GROUP BY 1, 2),
    e AS (
        SELECT to_char(x.expense_date, 'YYYY-MM') AS m, x.currency AS cur, sum(x.amount_minor) AS total
        FROM expenses x WHERE x.expense_date BETWEEN p_from AND p_to GROUP BY 1, 2),
    keys AS (SELECT m, cur FROM d UNION SELECT m, cur FROM y UNION SELECT m, cur FROM e)
    SELECT k.m, k.cur::text, COALESCE(d.inv, 0)::bigint, COALESCE(d.net, 0)::bigint, COALESCE(d.total, 0)::bigint,
           COALESCE(d.cn, 0)::bigint, COALESCE(y.pin, 0)::bigint, COALESCE(e.total, 0)::bigint
    FROM keys k
    LEFT JOIN d ON d.m = k.m AND d.cur = k.cur
    LEFT JOIN y ON y.m = k.m AND y.cur = k.cur
    LEFT JOIN e ON e.m = k.m AND e.cur = k.cur
    ORDER BY k.m, k.cur;
    PERFORM set_config('app.establishment_id', COALESCE(previous, ''), true);
END $$;

-- Sizes of one establishment (catalogue, contacts, documents), without any content.
CREATE FUNCTION nh_platform_establishment_counts(p_est uuid)
RETURNS TABLE (products bigint, customers bigint, suppliers bigint, documents bigint, drafts bigint)
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    previous text := current_setting('app.establishment_id', true);
BEGIN
    PERFORM set_config('app.establishment_id', p_est::text, true);
    RETURN QUERY SELECT
        (SELECT count(*) FROM products WHERE active)::bigint,
        (SELECT count(*) FROM parties WHERE is_customer AND active)::bigint,
        (SELECT count(*) FROM parties WHERE is_supplier AND active)::bigint,
        (SELECT count(*) FROM sales_documents WHERE status <> 'draft')::bigint,
        (SELECT count(*) FROM sales_documents WHERE status = 'draft')::bigint;
    PERFORM set_config('app.establishment_id', COALESCE(previous, ''), true);
END $$;

REVOKE ALL ON FUNCTION nh_platform_establishment_volumes(uuid, date, date) FROM PUBLIC;
REVOKE ALL ON FUNCTION nh_platform_establishment_monthly(uuid, date, date) FROM PUBLIC;
REVOKE ALL ON FUNCTION nh_platform_establishment_counts(uuid) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION nh_platform_establishment_volumes(uuid, date, date) TO nhill_app;
GRANT EXECUTE ON FUNCTION nh_platform_establishment_monthly(uuid, date, date) TO nhill_app;
GRANT EXECUTE ON FUNCTION nh_platform_establishment_counts(uuid) TO nhill_app;

INSERT INTO platform_settings (key, value, description) VALUES
    ('platform_stats_notice', '{"fr": "Nhill calcule des statistiques agrégées (nombre de documents, volumes par devise) pour l''exploitation du service. Le détail des clients, produits et lignes de documents n''est pas accessible à l''équipe Nhill.", "en": "Nhill computes aggregated statistics (document counts, volumes per currency) to operate the service. Customer, product and document line details are not accessible to the Nhill team."}',
     'Notice shown to establishments about aggregated platform statistics')
ON CONFLICT (key) DO NOTHING;
