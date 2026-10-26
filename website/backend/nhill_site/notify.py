"""Optional e-mail notification to the team when a form is received (SMTP, STARTTLS).

Only the form type and the submission identifier are sent: the content stays in the
database and is read with the maintenance CLI. Without SITE_SMTP_HOST, nothing is sent.
"""
import logging
import smtplib
import ssl
from email.message import EmailMessage

log = logging.getLogger("nhill_site")
LABELS = {"demo": "demande de démonstration", "pilot": "candidature au programme pilote",
          "partner": "proposition de partenariat", "contact": "question"}


def new_submission(cfg, kind, submission_id):
    if not (cfg.smtp_host and cfg.notify_email and cfg.smtp_from):
        return False
    msg = EmailMessage()
    msg["Subject"] = f"Nhill — nouvelle {LABELS.get(kind, kind)}"
    msg["From"] = cfg.smtp_from
    msg["To"] = cfg.notify_email
    msg.set_content(f"Nouvelle {LABELS.get(kind, kind)} reçue sur le site.\n\n"
                    f"Identifiant : {submission_id}\n"
                    f"Lecture : python3 -m nhill_site.cli show {submission_id}\n")
    try:
        with smtplib.SMTP(cfg.smtp_host, cfg.smtp_port, timeout=15) as s:
            s.starttls(context=ssl.create_default_context())
            if cfg.smtp_user:
                s.login(cfg.smtp_user, cfg.smtp_password)
            s.send_message(msg)
        return True
    except (OSError, smtplib.SMTPException) as e:
        log.warning("notification e-mail not sent: %s", e.__class__.__name__)
        return False
