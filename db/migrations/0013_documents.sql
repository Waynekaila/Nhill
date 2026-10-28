-- Nhill migration 0013 — sales documents (3.0.0).
--   Mentions printed on every sales document of the establishment (A4 and ticket): set by
--   the establishment in Nhill Business (e.g. legal mentions its activity requires). Nhill
--   does not write them for the establishment and does not check their legal content.
--   Invoices issued at the till (Nhill POS) use the existing invoice type, numbering
--   sequence and rules: no new document type is created.
ALTER TABLE establishment_settings ADD COLUMN document_mentions text NOT NULL DEFAULT ''
    CHECK (length(document_mentions) <= 1000);
