-- Nhill migration 0004 — reference data (true data, not samples).
-- Currencies: ISO 4217 codes and minor units.
INSERT INTO currencies (code, minor_units, name_fr, name_en) VALUES
    ('CDF', 2, 'Franc congolais', 'Congolese franc'),
    ('USD', 2, 'Dollar américain', 'US dollar'),
    ('EUR', 2, 'Euro', 'Euro'),
    ('XAF', 0, 'Franc CFA (CEMAC)', 'CFA franc (CEMAC)'),
    ('XOF', 0, 'Franc CFA (UEMOA)', 'CFA franc (WAEMU)'),
    ('RWF', 0, 'Franc rwandais', 'Rwandan franc'),
    ('ZAR', 2, 'Rand sud-africain', 'South African rand'),
    ('KES', 2, 'Shilling kényan', 'Kenyan shilling'),
    ('GBP', 2, 'Livre sterling', 'Pound sterling'),
    ('CHF', 2, 'Franc suisse', 'Swiss franc');

-- Business rules from the product specification, kept configurable.
INSERT INTO platform_settings (key, value, description) VALUES
    ('trial_days',               '30',  'Durée de l''essai gratuit (jours)'),
    ('grace_days',               '7',   'Période de grâce après l''échéance (jours)'),
    ('reminder_days_before',     '7',   'Premier rappel avant l''échéance (jours)'),
    ('code_rotation_days',       '30',  'Durée de validité d''un code d''établissement (jours)'),
    ('code_check_max_per_15min', '10',  'Tentatives de validation de code par IP et par 15 minutes'),
    ('login_max_per_15min',      '8',   'Tentatives de connexion par compte et par 15 minutes'),
    ('invitation_days',          '7',   'Validité d''une invitation (jours)'),
    ('suspension_policy',        '"read_only"', 'Après la grâce : lecture et export seuls');
