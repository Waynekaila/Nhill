# Message templates (WhatsApp, e-mail)

*Français : [../fr/06-modeles-messages.md](../fr/06-modeles-messages.md)*

Common rules: only send to someone who agreed to this channel, or who wrote first; sign with
a first name; at most two follow-ups (D+3, then D+10), then stop; unsubscribe mention in
commercial messages; never announce a price, an availability date or a feature that is not
in the written proposal or in the software.

## Merge fields

Each message is a template: the fields in braces are replaced, when the message is sent, by
the recipient's details. They are not missing information in this document; their source
is given below.

| Field | Content | Source |
|---|---|---|
| `{contact}` | the person's first name, or "Mr" / "Ms" followed by the surname | prospect tracker, `contact_name` column |
| `{sender}` | first name and surname of the team member writing | — |
| `{business}` | name of the shop, company or organisation | `business` column |
| `{appointment}` | agreed day, time and place | `next_action`, `next_action_date` columns |
| `{slots}` | two free slots of the team | team calendar |
| `{meeting_date}` | date of the previous meeting | `last_action` column |
| `{workshop_date}` | proposed date of the workshop | team calendar |
| `{places}` | number of trial places reserved for this organisation | team decision, noted in the partner tracker |
| `{trial_start}`, `{trial_end}` | first and last day of the 30-day trial | accepted trial terms |
| `{interests}`, `{concerns}`, `{answers}` | points noted during the demo | demo record ([`07`](07-demo-script.md)) |
| `{benefit}` | benefit mentioned by the business, in its own words | end-of-trial questionnaire ([`08`](08-sales-process-and-tracking.md) § 5) |
| `{review}` | before and after measurements, as measured | trial report ([`02`](02-pilot-programme.md) § 4) |
| `{fixes}` | fixes made thanks to the business's feedback | trial issue log |
| `{access_end}` | end date of full access | Nhill Console, business record |

## 1. WhatsApp — after a visit (consent given)

> Hello {contact}, it's {sender} from Nhill, we met today at {business}. Thank you for your
> welcome.
> Here is the presentation sheet I promised (attached). For the free demo with your
> products (15 min), just tell me which day suits you.
> To stop receiving messages from Nhill, reply STOP.

Attachment: `marketing/visuals/export/trial-sheet-a5-en.pdf` (assisted free trial).

## 2. WhatsApp — demo confirmation

> Hello {contact}, I confirm our demo: {appointment}. If you can, prepare a list of the 10 to
> 20 items you sell most: I'll show you Nhill with them. See you soon!

## 3. WhatsApp — follow-up (D+3, then D+10 without an answer)

> Hello {contact}, I'm getting back to you about Nhill. Would you still like to see the
> demo? If now isn't the right time, no problem, just tell me and I won't follow up.

## 4. E-mail — first contact after a website form

**Subject**: Your Nhill demo request

> Hello {contact},
>
> Thank you for requesting a Nhill demo for {business}.
>
> I suggest 30 minutes, at your shop or by video call, with your own products. Here are two
> slots: {slots}. If neither suits you, suggest another one.
>
> To prepare, could you tell me: how many people sell, which devices you have (computer,
> Android tablet), and what you'd like to improve first (till, stock, credit sales)?
>
> Kind regards,
> {sender} — Nhill
> info@nhill.online · nhill.online

## 5. E-mail — meeting request to an organisation (association, incubator)

**Subject**: Meeting proposal — management tool for your trader members

> Dear Sir or Madam,
>
> I am {sender}, from Nhill, software to manage sales, stock and the till, designed for
> traders and SMEs in the DRC (French and English, CDF and USD).
>
> In Kinshasa we offer a 30-day assisted free trial, with installation and training. It
> could interest the members of {business} who want to keep better track of their till and
> stock.
>
> Would you accept a 30-minute conversation to see whether working together makes sense? I
> can come to your office, whenever suits you.
>
> Important: Nhill is management software; it is not certified by the DGI for the
> standardised invoice.
>
> Kind regards,
> {sender} — Nhill Solution — info@nhill.online

## 6. Collaboration proposal (after the meeting)

**Subject**: Nhill × {business} — proposal

> Hello {contact},
>
> Thank you for our conversation on {meeting_date}. As agreed, here is our proposal:
>
> 1. **A 90-minute workshop** for your members: keeping track of your till and stock, with a
>    Nhill demo. Proposed date: {workshop_date}.
> 2. **Reserved places** in the assisted free trial for {places} of your members.
> 3. **A partnership**, if you wish: its terms (commission on your members' subscriptions,
>    or a discount for them) are set in writing, in a letter of agreement, before anything
>    starts.
>
> What we ask: share the workshop invitation with your members (without passing us their
> details without their consent).
>
> Happy to discuss.
> {sender} — Nhill

Point 3 refers to the partnership terms
([`legal/en/10-partnership-agreement.md`](../../legal/en/10-partnership-agreement.md)).

## 7. Referral request (satisfied business, week 9)

> Hello {contact}, thank you again for taking part in the trial. You told me that
> {benefit}. Do you know one or two traders Nhill could help in the same way? If so, ask
> them first whether they agree, then send me their name and number, or give them our
> address: info@nhill.online.

## 8. Post-demo follow-up

**Subject**: Following our demo — next steps

> Hello {contact},
>
> Thank you for your time today. To sum up:
> - what interests you: {interests};
> - what worries you: {concerns} — {answers};
> - proposal: 30-day assisted free trial, from {trial_start} to {trial_end}.
>
> I attach the assisted free trial terms for you to read at your own pace. If they suit
> you, we'll set the onboarding (half a day).
>
> {sender} — Nhill

Attachment: [`legal/pdf/en/09-assisted-trial-terms.pdf`](../../legal/pdf/en/09-assisted-trial-terms.pdf).

## 9. End of trial — follow-up proposal

**Subject**: Your Nhill trial review and proposal

> Hello {contact},
>
> Here is the review of your 30-day trial: {review}.
>
> To continue, please find attached our written proposal: plan, term, price, currency and
> payment methods. Nothing is invoiced without your agreement. Without a reply, full access
> stops on {access_end}; Nhill then becomes read-only and you keep 90 days to export your
> data.
>
> Thank you for your feedback, which helped us fix {fixes}.
> {sender} — Nhill

## 10. E-mail signature

See `marketing/visuals/signature-email-en.html` (editable source).
