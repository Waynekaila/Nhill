# Prospecting scripts (shop visit, call, WhatsApp voice note)

*Français : [../fr/05-scripts-prospection.md](../fr/05-scripts-prospection.md)*

Principles: short, polite, concrete; talk about the trader's problem before the software;
promise nothing Nhill does not do; ask for consent before recording a number for messages
(template `legal/en/16-marketing-consent.md`). Adapt the language: French, Lingala or English
depending on the person. Most visits in Kinshasa will be in French or Lingala; these English
versions serve English-speaking prospects and partners.

The fields in braces are replaced at the time of the call or visit: `{sender}` = your first
name; `{referrer}` = name of the person who recommended the contact (prospect tracker,
`source` and `notes` columns). The other fields are described in
[`06-message-templates.md`](06-message-templates.md).

## 1. Shop visit (2 minutes)

> Good morning Madam (or Sir), my name is {sender}, from Nhill. I'll only take two minutes —
> I can see you have customers.
>
> Nhill is software that lets traders keep track of their sales, stock and till on a
> computer, a tablet or a phone. For example, in the evening, you see how much each seller
> collected, without redoing the notebook sums.
>
> Does the evening till take you time, or do you sometimes have differences?
>
> *Listen. Ask 2 or 3 discovery questions (see `04` § 4).*
>
> We offer a 30-day assisted free trial, with installation and training. I can show you in
> fifteen minutes, with your own products, when you're less busy. Which day suits you?

**If "I don't have time"**: "I understand. When can I come back: early or late in the week?
Or I'll leave you this sheet, and if you allow me to write to you on WhatsApp, I'll also
send it as a PDF."

**If "it's expensive"**: "The 30-day trial is free and without commitment. Afterwards, you
receive a written proposal suited to your business; nothing is invoiced without your
agreement."

**If "I'm VAT-registered"**: "Nhill doesn't replace your DGI standardised-invoice device. It
helps with stock, the till and tracking sellers; you keep your device for the tax invoice."

**If "what if the Internet cuts?"**: "Nhill needs the Internet to record. If the connection
drops in the middle of a sale, you retry it and it isn't counted twice. For a long outage,
keep a backup notebook; we enter it afterwards."

**If "my data?"**: "It's yours. Each business is kept separate from the others in the
database, and our team doesn't see your invoices or customers. You can export everything
whenever you want."

## 2. Phone call (after a referral)

> Hello Madam (or Sir), {sender} from Nhill. {referrer} gave me your number and said you
> might be interested in a tool to track the till and stock. Do you have two minutes?
>
> *If yes:* How do you manage the till today? … I suggest a fifteen-minute demo at your
> shop, with your products. Tuesday or Thursday?
>
> *If no:* May I send you our presentation sheet on WhatsApp, if you agree? *Record the
> consent, date and channel.*

Only call if the referrer asked the person whether they agree to be called (document
`legal/en/16`, § 4).

## 3. WhatsApp voice note (30 seconds, only with consent)

> Hello, it's {sender} from Nhill, thanks for agreeing. As promised: Nhill lets you see your
> sales, your stock and each seller's till, even when you're not in the shop. I'll send the
> presentation sheet right after. If you'd like the free demo with your products, just
> reply with a day that suits you. Have a good day.

## 4. After the contact

| Step | Deadline |
|---|---|
| Enter the contact in `trackers/prospect-tracker.csv` (status, score, consent) | same day |
| Send what was promised (presentation sheet `marketing/visuals/export/trial-sheet-a5-en.pdf`) | within 24 h |
| Follow up if no answer | D+3, then D+10, then stop |
