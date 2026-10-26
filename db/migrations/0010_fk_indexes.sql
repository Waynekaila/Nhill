-- Nhill migration 0010 — indexes on the foreign keys used by frequent queries
-- (returns of a receipt, sessions of a register, stock per warehouse, payments per
-- customer or account, product usage before deletion). Reported by the Supabase
-- performance advisor; low-volume reference keys (currencies, plans, audit authors)
-- are left without an index on purpose.

CREATE INDEX IF NOT EXISTS sales_documents_related ON sales_documents (establishment_id, related_document_id)
    WHERE related_document_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS sales_document_lines_product ON sales_document_lines (establishment_id, product_id)
    WHERE product_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS document_status_history_est_doc ON document_status_history (establishment_id, document_id);
CREATE INDEX IF NOT EXISTS purchase_lines_product ON purchase_lines (establishment_id, product_id) WHERE product_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS purchases_supplier ON purchases (establishment_id, supplier_id);
CREATE INDEX IF NOT EXISTS purchases_warehouse ON purchases (establishment_id, warehouse_id);
CREATE INDEX IF NOT EXISTS payments_party ON payments (establishment_id, party_id) WHERE party_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS payments_account ON payments (establishment_id, account_id) WHERE account_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS expenses_supplier ON expenses (establishment_id, supplier_id) WHERE supplier_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS expenses_category ON expenses (establishment_id, category_id) WHERE category_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS products_category ON products (establishment_id, category_id) WHERE category_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS products_tax_rate ON products (establishment_id, tax_rate_id) WHERE tax_rate_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS stock_levels_warehouse ON stock_levels (establishment_id, warehouse_id);
CREATE INDEX IF NOT EXISTS stock_movements_warehouse ON stock_movements (establishment_id, warehouse_id);
CREATE INDEX IF NOT EXISTS pos_registers_warehouse ON pos_registers (establishment_id, warehouse_id);
CREATE INDEX IF NOT EXISTS pos_register_users_register ON pos_register_users (establishment_id, register_id);
CREATE INDEX IF NOT EXISTS pos_sessions_register ON pos_sessions (establishment_id, register_id);
CREATE INDEX IF NOT EXISTS pos_approvals_session ON pos_approvals (establishment_id, session_id) WHERE session_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS sales_promo_redemptions_document ON sales_promo_redemptions (establishment_id, document_id);
CREATE INDEX IF NOT EXISTS subscriptions_plan ON subscriptions (plan_id);
CREATE INDEX IF NOT EXISTS subscription_orders_plan ON subscription_orders (plan_id);
CREATE INDEX IF NOT EXISTS support_tickets_assigned ON support_tickets (assigned_to) WHERE assigned_to IS NOT NULL;
