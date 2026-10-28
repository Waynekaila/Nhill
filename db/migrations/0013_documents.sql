-- Nhill migration 0013 — sales documents (3.0.0).
--   Mentions printed on every sales document of the establishment (A4 and ticket): set by
--   the establishment in Nhill Business (e.g. legal mentions its activity requires). Nhill
--   does not write them for the establishment and does not check their legal content.
--   Invoices issued at the till (Nhill POS) use the existing invoice type, numbering
--   sequence and rules: no new document type is created.
ALTER TABLE establishment_settings ADD COLUMN document_mentions text NOT NULL DEFAULT ''
    CHECK (length(document_mentions) <= 1000);

-- International use: more currencies (ISO 4217 codes and minor units; names from the
-- Unicode CLDR). Existing rows are left untouched.
INSERT INTO currencies (code, minor_units, name_fr, name_en) VALUES
    ('AED', 2, 'Dirham des Émirats arabes unis', 'UAE dirham'),
    ('AOA', 2, 'Kwanza angolais', 'Angolan kwanza'),
    ('AUD', 2, 'Dollar australien', 'Australian dollar'),
    ('BIF', 0, 'Franc burundais', 'Burundian franc'),
    ('BRL', 2, 'Réal brésilien', 'Brazilian real'),
    ('CAD', 2, 'Dollar canadien', 'Canadian dollar'),
    ('CNY', 2, 'Yuan renminbi chinois', 'Chinese yuan'),
    ('DKK', 2, 'Couronne danoise', 'Danish krone'),
    ('DZD', 2, 'Dinar algérien', 'Algerian dinar'),
    ('EGP', 2, 'Livre égyptienne', 'Egyptian pound'),
    ('ETB', 2, 'Birr éthiopien', 'Ethiopian birr'),
    ('GHS', 2, 'Cédi ghanéen', 'Ghanaian cedi'),
    ('GNF', 0, 'Franc guinéen', 'Guinean franc'),
    ('INR', 2, 'Roupie indienne', 'Indian rupee'),
    ('JPY', 0, 'Yen japonais', 'Japanese yen'),
    ('MAD', 2, 'Dirham marocain', 'Moroccan dirham'),
    ('MZN', 2, 'Metical mozambicain', 'Mozambican metical'),
    ('NGN', 2, 'Naira nigérian', 'Nigerian naira'),
    ('NOK', 2, 'Couronne norvégienne', 'Norwegian krone'),
    ('PLN', 2, 'Zloty polonais', 'Polish zloty'),
    ('SAR', 2, 'Riyal saoudien', 'Saudi riyal'),
    ('SEK', 2, 'Couronne suédoise', 'Swedish krona'),
    ('TND', 3, 'Dinar tunisien', 'Tunisian dinar'),
    ('TZS', 2, 'Shilling tanzanien', 'Tanzanian shilling'),
    ('UGX', 0, 'Shilling ougandais', 'Ugandan shilling'),
    ('ZMW', 2, 'Kwacha zambien', 'Zambian kwacha')
ON CONFLICT (code) DO NOTHING;
