#!/usr/bin/env python3
"""Seeds a DEMONSTRATION establishment through the public API (development only).

Every name, amount and rate created here is fictional and labelled "démo".
Never run this against a production server.

Usage: seed-demo.py [--server http://127.0.0.1:8080] [--email demo@example.cd] [--password ...]
Prints the owner credentials and the establishment code at the end.
"""
import argparse
import datetime
import json
import sys
import urllib.error
import urllib.request
import uuid


class Api:
    def __init__(self, base):
        self.base = base.rstrip("/")
        self.token = None

    def call(self, method, path, body=None, idem=False):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.base + path, data=data, method=method)
        req.add_header("Accept", "application/json")
        req.add_header("Accept-Language", "fr")
        if data is not None:
            req.add_header("Content-Type", "application/json")
        if self.token:
            req.add_header("Authorization", "Bearer " + self.token)
        if idem:
            req.add_header("Idempotency-Key", str(uuid.uuid4()))
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                raw = r.read()
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as e:
            sys.exit(f"{method} {path} -> {e.code}: {e.read().decode(errors='replace')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--server", default="http://127.0.0.1:8080")
    ap.add_argument("--email", default=f"demo-{uuid.uuid4().hex[:6]}@example.cd")
    ap.add_argument("--password", default="Demo-Nhill-2026")
    a = ap.parse_args()
    if not (a.server.startswith("http://127.0.0.1") or a.server.startswith("http://localhost")):
        sys.exit("refusing to seed demo data on a non-local server")

    api = Api(a.server)
    created = api.call("POST", "/api/v1/establishments", {
        "kind": "company",
        "trade_name": "Quincaillerie Exemple (démo)",
        "legal_name": "EXEMPLE DÉMO SARL",
        "country_code": "CD", "city": "Kinshasa", "address": "Avenue Exemple 1, Gombe (adresse fictive)",
        "preferred_language": "fr", "main_currency": "CDF", "vat_registered": True,
        "sector": "Commerce de détail",
        "legal_ids": {"rccm": f"CD/KNG/RCCM/26-B-{uuid.uuid4().int % 99999:05d}",
                      "id_nat": f"01-{uuid.uuid4().int % 9999:04d}-N{uuid.uuid4().int % 99999:05d}X",
                      "nif": f"A{uuid.uuid4().int % 9999999:07d}Z"},
        "owner": {"full_name": "Utilisateur Démo", "email": a.email, "password": a.password},
    })
    api.token = created["session"]["access_token"]

    api.call("PATCH", "/api/v1/establishment/settings", {
        "payment_instructions": "Paiement par virement ou Mobile Money — coordonnées fictives (démo)",
        "default_payment_terms_days": 15})

    cust = [api.call("POST", "/api/v1/parties", {"is_customer": True, "name": n, "phone": "+243810000000", "city": "Kinshasa"})["id"]
            for n in ("Client Démo Alpha", "Client Démo Beta", "Client Démo Gamma")]
    supplier = api.call("POST", "/api/v1/parties", {"is_supplier": True, "name": "Fournisseur Démo", "city": "Lubumbashi"})["id"]

    products = {}
    for sku, name, price, cost in (("CIM-50", "Ciment 50 kg (démo)", "18500.00", "14000.00"),
                                   ("TOL-3M", "Tôle ondulée 3 m (démo)", "32000.00", "25500.00"),
                                   ("PEI-20", "Peinture 20 L (démo)", "95000.00", "71000.00"),
                                   ("CLO-1K", "Clous 1 kg (démo)", "6500.00", "4200.00")):
        products[sku] = api.call("POST", "/api/v1/products", {"sku": sku, "name": name, "kind": "good", "sale_price": price,
                                                             "purchase_price": cost, "reorder_level": "10", "unit": "unit"})["id"]
    products["LIV"] = api.call("POST", "/api/v1/products", {"sku": "LIV", "name": "Livraison (démo)", "kind": "service",
                                                           "sale_price": "15000.00", "unit": "service"})["id"]
    for sku, q in (("CIM-50", "120"), ("TOL-3M", "40"), ("PEI-20", "8"), ("CLO-1K", "60")):
        api.call("POST", "/api/v1/stock/adjustments", {"product_id": products[sku], "quantity": q,
                                                       "reason": "Stock d'ouverture (démo)", "kind": "opening"}, idem=True)

    purchase = api.call("POST", "/api/v1/purchases", {"supplier_id": supplier, "supplier_reference": "FDEMO-0042", "lines": [
        {"product_id": products["CIM-50"], "quantity": "50", "unit_cost": "14000.00", "tax_rate_bp": 1600}]}, idem=True)
    api.call("POST", f"/api/v1/purchases/{purchase['id']}/receive", {}, idem=True)

    # A seller employee (invited by the owner, activated with the establishment code).
    seller_email = a.email.replace("@", "+vendeur@")
    inv = api.call("POST", "/api/v1/invitations", {"email": seller_email, "role": "sales"}, idem=True)
    seller = Api(a.server)
    ticket = seller.call("POST", "/api/v1/join/check-code", {"code": created["establishment_code"]})["join_ticket"]
    seller.token = seller.call("POST", "/api/v1/join/activate", {"join_ticket": ticket, "email": seller_email,
                                                                 "activation_code": inv["activation_code"],
                                                                 "full_name": "Vendeur Démo", "password": a.password})["access_token"]

    def invoice(customer, lines, doc_type="invoice", currency=None, by=None):
        b = {"doc_type": doc_type, "party_id": customer, "lines": lines}
        if currency:
            b["currency"] = currency
        return (by or api).call("POST", "/api/v1/sales-documents", b, idem=True)

    paid = invoice(cust[0], [{"product_id": products["CIM-50"], "quantity": "20"}, {"product_id": products["LIV"], "quantity": "1"}])
    paid = api.call("POST", f"/api/v1/sales-documents/{paid['id']}/issue", {}, idem=True)
    api.call("POST", "/api/v1/payments", {"document_id": paid["id"], "amount": paid["total"], "method": "mobile_money",
                                          "reference": "DEMO-MM-001"}, idem=True)

    partial = invoice(cust[1], [{"product_id": products["TOL-3M"], "quantity": "12"}, {"product_id": products["CLO-1K"], "quantity": "5", "discount_bp": 500}], by=seller)
    partial = seller.call("POST", f"/api/v1/sales-documents/{partial['id']}/issue", {}, idem=True)
    seller.call("POST", "/api/v1/payments", {"document_id": partial["id"], "amount": "150000.00", "method": "cash"}, idem=True)

    issued = invoice(cust[2], [{"product_id": products["PEI-20"], "quantity": "3"}], by=seller)
    seller.call("POST", f"/api/v1/sales-documents/{issued['id']}/issue", {}, idem=True)

    invoice(cust[0], [{"description": "Devis chantier (démo)", "quantity": "1", "unit_price": "450000.00", "tax_rate_bp": 1600}], "quote")
    invoice(cust[2], [{"product_id": products["CLO-1K"], "quantity": "2"}])  # draft

    for desc, amount, cat in (("Loyer du dépôt — octobre (démo)", "800000.00", "Loyer"),
                              ("Carburant livraison (démo)", "120000.00", "Transport")):
        cats = {c["name"]: c["id"] for c in api.call("GET", "/api/v1/expense-categories")["items"]}
        api.call("POST", "/api/v1/expenses", {"description": desc, "amount": amount, "currency": "CDF",
                                              "category_id": cats.get(cat), "paid": {"method": "cash"}}, idem=True)
    api.call("POST", "/api/v1/expenses", {"description": "Facture électricité (démo)", "amount": "95000.00", "currency": "CDF"}, idem=True)

    api.call("POST", "/api/v1/exchange-rates", {"from": "USD", "to": "CDF", "rate": "2800.0000", "rate_date": datetime.date.today().isoformat(),
                                                "source": "Valeur de démonstration — non officielle"})
    # Caisse de démonstration (Nhill POS) : sans caisse, la visite de Nhill POS s'arrête à
    # « Aucune caisse ne vous est attribuée ».
    api.call("POST", "/api/v1/pos/registers", {"code": "C1", "name": "Caisse principale (démo)", "location": "Comptoir"})

    print(json.dumps({"email": a.email, "seller_email": seller_email, "password": a.password, "establishment_code": created["establishment_code"],
                      "establishment_id": created["establishment"]["id"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
