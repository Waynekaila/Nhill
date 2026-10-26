-- Nhill migration 0003 — row-level security and privileges of the application role.
--
-- Business tables: rows are visible/writable only when establishment_id equals
-- the transaction setting app.establishment_id (set by the server for every
-- request). FORCE applies the policy to the table owner too. There is no bypass
-- for platform administrators.

DO $$
DECLARE
    t text;
    business_tables text[] := ARRAY[
        'establishment_settings', 'objects', 'tax_rates', 'product_categories', 'products',
        'warehouses', 'stock_levels', 'stock_movements', 'parties', 'sales_documents',
        'sales_document_lines', 'document_sequences', 'document_status_history', 'purchases',
        'purchase_lines', 'cash_accounts', 'expense_categories', 'expenses', 'payments',
        'exchange_rates', 'idempotency_keys'];
BEGIN
    FOREACH t IN ARRAY business_tables LOOP
        EXECUTE format('ALTER TABLE %I ENABLE ROW LEVEL SECURITY', t);
        EXECUTE format('ALTER TABLE %I FORCE ROW LEVEL SECURITY', t);
        EXECUTE format('CREATE POLICY %I ON %I USING (establishment_id = nh_current_establishment()) '
                       'WITH CHECK (establishment_id = nh_current_establishment())', t || '_tenant', t);
    END LOOP;
END $$;

-- ---------------------------------------------------------------- privileges
-- Everything lives in the dedicated schema "nhill" (not exposed by the Supabase
-- Data API). Only the application role may use it; PUBLIC (and therefore the
-- Supabase roles anon / authenticated) has no access at all.
REVOKE ALL ON SCHEMA nhill FROM PUBLIC;
GRANT USAGE ON SCHEMA nhill TO nhill_app;
REVOKE ALL ON ALL TABLES IN SCHEMA nhill FROM nhill_app;
REVOKE ALL ON ALL FUNCTIONS IN SCHEMA nhill FROM PUBLIC;

-- Reference / platform tables
GRANT SELECT ON currencies, compliance_profiles, platform_settings TO nhill_app;
GRANT INSERT, UPDATE ON compliance_profiles, platform_settings TO nhill_app;
GRANT INSERT, UPDATE ON currencies TO nhill_app;
GRANT SELECT, INSERT, UPDATE ON establishments, establishment_codes, users, memberships, invitations,
    sessions, plans, subscriptions, subscription_orders, admin_users, admin_sessions, announcements,
    notifications, email_outbox, attachment_requests, trial_registry TO nhill_app;
GRANT SELECT, INSERT, DELETE ON establishment_identifiers TO nhill_app;
GRANT SELECT, INSERT, UPDATE, DELETE ON join_tickets, auth_throttle TO nhill_app;

-- Append-only histories
GRANT SELECT, INSERT ON audit_log, subscription_events, document_status_history, stock_movements TO nhill_app;
GRANT SELECT, INSERT ON payment_webhook_events TO nhill_app;
GRANT UPDATE (processed_at, outcome) ON payment_webhook_events TO nhill_app;
GRANT SELECT ON usage_daily TO nhill_app;

-- Stock levels are read-only for the application (maintained by trigger)
GRANT SELECT ON stock_levels TO nhill_app;

-- Business tables
GRANT SELECT, INSERT, UPDATE ON establishment_settings, tax_rates, product_categories, products,
    warehouses, parties, sales_documents, document_sequences, purchases, cash_accounts,
    expense_categories, expenses, payments, exchange_rates, idempotency_keys TO nhill_app;
GRANT SELECT, INSERT, UPDATE, DELETE ON sales_document_lines, purchase_lines, objects TO nhill_app;
-- DELETE of documents/purchases is limited to drafts by the guard triggers
GRANT DELETE ON sales_documents, purchases, product_categories, expense_categories, idempotency_keys TO nhill_app;

GRANT USAGE ON ALL SEQUENCES IN SCHEMA nhill TO nhill_app;
GRANT EXECUTE ON FUNCTION nh_current_establishment() TO nhill_app;
