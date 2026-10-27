/* Nhill — site vitrine. JavaScript sans dépendance.
   - menu mobile
   - formulaires : validation, envoi JSON au backend configuré dans config/site.json.
     Si aucun backend n'est configuré, le formulaire le dit clairement et n'envoie rien
     (aucun faux « message envoyé »). */
(function () {
  "use strict";
  var lang = document.documentElement.lang === "en" ? "en" : "fr";
  var T = {
    fr: {
      required: "Ce champ est obligatoire.",
      email: "Saisissez une adresse e-mail valide, par exemple nom@entreprise.cd.",
      phone: "Saisissez un numéro de téléphone valide, par exemple +243 81 234 5678.",
      consent: "Cochez cette case pour que nous puissions traiter votre demande.",
      sending: "Envoi en cours…",
      sent: "Demande reçue. Nous vous répondrons par e-mail ou par téléphone.",
      offline: "Ce formulaire n'est pas encore relié à notre serveur : votre demande n'a pas été envoyée.",
      offlineMail: " Écrivez-nous à ",
      failed: "La demande n'a pas pu être envoyée. Vérifiez votre connexion et réessayez.",
      tooFast: "Patientez quelques secondes avant d'envoyer le formulaire.",
      tooMany: "Trop de demandes depuis votre connexion. Réessayez dans une heure.",
      menu: "Menu", close: "Fermer"
    },
    en: {
      required: "This field is required.",
      email: "Enter a valid e-mail address, for example name@company.com.",
      phone: "Enter a valid phone number, for example +243 81 234 5678.",
      consent: "Tick this box so that we can process your request.",
      sending: "Sending…",
      sent: "Request received. We will answer by e-mail or phone.",
      offline: "This form is not connected to our server yet: your request was not sent.",
      offlineMail: " Write to us at ",
      failed: "The request could not be sent. Check your connection and try again.",
      tooFast: "Wait a few seconds before sending the form.",
      tooMany: "Too many requests from your connection. Try again in an hour.",
      menu: "Menu", close: "Close"
    }
  }[lang];

  // ---------------------------------------------------------------- menu
  var toggle = document.querySelector(".nav-toggle");
  var nav = document.getElementById("nav");
  if (toggle && nav) {
    toggle.addEventListener("click", function () {
      var open = nav.classList.toggle("open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
      toggle.textContent = open ? T.close : T.menu;
    });
  }

  // ---------------------------------------------------------------- prefilled messages
  // Links such as contact.html?sujet=formule#question put a first sentence in the message
  // of the targeted form, so that the visitor only has to complete it. Fixed texts only:
  // nothing from the address bar is ever inserted in the page.
  var PREFILL = {
    fr: { formule: "Je souhaite discuter d'une formule adaptée à mon entreprise : ",
          boutique: "Activité : boutique avec caisse. ", quincaillerie: "Activité : quincaillerie ou dépôt. ",
          services: "Activité : prestataire de services. ", pme: "Activité : PME avec plusieurs employés. " },
    en: { plan: "I would like to discuss a plan suited to my business: ",
          shop: "Business: shop with a till. ", hardware: "Business: hardware store or depot. ",
          services: "Business: service provider. ", sme: "Business: SME with several employees. " }
  }[lang];
  (function () {
    var m = /[?&](?:sujet|topic)=([a-z]+)/.exec(location.search);
    if (!m || !PREFILL[m[1]]) return;
    var target = location.hash ? document.getElementById(location.hash.slice(1)) : null;
    var scope = target ? target.closest(".section") || document : document;
    var area = scope.querySelector("form.lead textarea");
    if (area && !area.value) area.value = PREFILL[m[1]];
  })();

  // ---------------------------------------------------------------- forms
  var forms = document.querySelectorAll("form.lead");
  if (!forms.length) return;
  var root = document.body.getAttribute("data-root") || ".";
  var config = null;
  var configLoaded = fetch(root + "/config/site.json", { cache: "no-store" })
    .then(function (r) { return r.ok ? r.json() : {}; })
    .catch(function () { return {}; })
    .then(function (c) { config = c || {}; return config; });

  var emailRe = /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/;
  var phoneRe = /^\+?[0-9 ().-]{8,20}$/;

  function setError(input, msg) {
    var box = document.getElementById(input.id + "-err");
    input.setAttribute("aria-invalid", msg ? "true" : "false");
    if (box) box.textContent = msg || "";
  }
  function validate(form) {
    var first = null;
    form.querySelectorAll("input, select, textarea").forEach(function (el) {
      if (el.classList.contains("hp-input") || el.type === "hidden") return;
      var msg = "";
      var v = el.type === "checkbox" ? el.checked : el.value.trim();
      if (el.required && !v) msg = el.type === "checkbox" ? T.consent : T.required;
      else if (el.type === "email" && v && !emailRe.test(v)) msg = T.email;
      else if (el.type === "tel" && v && !phoneRe.test(v)) msg = T.phone;
      setError(el, msg);
      if (msg && !first) first = el;
    });
    if (first) first.focus();
    return !first;
  }
  function status(form, kind, text) {
    var s = form.querySelector(".form-status");
    s.className = "form-status " + kind;
    s.textContent = "";
    s.appendChild(document.createTextNode(text));
    if (kind === "offline" && config && config.contactEmail) {
      s.appendChild(document.createTextNode(T.offlineMail));
      var a = document.createElement("a");
      a.href = "mailto:" + config.contactEmail;
      a.textContent = config.contactEmail;
      s.appendChild(a);
      s.appendChild(document.createTextNode("."));
    }
    s.setAttribute("tabindex", "-1");
    s.focus();
  }

  forms.forEach(function (form) {
    var started = Date.now();
    form.addEventListener("submit", function (ev) {
      ev.preventDefault();
      if (!validate(form)) return;
      configLoaded.then(function () {
        var base = (config.apiBaseUrl || "").replace(/\/+$/, "");
        if (!base) { status(form, "offline", T.offline); return; }
        var data = { kind: form.getAttribute("data-kind"), lang: lang, elapsed_ms: Date.now() - started };
        new FormData(form).forEach(function (value, key) { data[key] = typeof value === "string" ? value.trim() : value; });
        form.querySelectorAll('input[type="checkbox"]').forEach(function (c) { data[c.name] = c.checked; });
        var btn = form.querySelector('button[type="submit"]');
        btn.disabled = true;
        var label = btn.textContent;
        btn.textContent = T.sending;
        fetch(base + "/api/forms/" + encodeURIComponent(data.kind), {
          method: "POST",
          headers: { "Content-Type": "application/json", "Accept": "application/json" },
          body: JSON.stringify(data)
        }).then(function (r) {
          return r.json().catch(function () { return {}; }).then(function (j) { return { status: r.status, body: j }; });
        }).then(function (res) {
          if (res.status === 201) { form.reset(); status(form, "ok", T.sent); return; }
          if (res.status === 429) { status(form, "error", T.tooMany); return; }
          if (res.status === 422 && res.body && res.body.field) {
            var el = form.querySelector('[name="' + res.body.field + '"]');
            if (el) { setError(el, res.body.field === "elapsed_ms" ? T.tooFast : (el.type === "email" ? T.email : T.required)); el.focus(); }
            if (res.body.field === "elapsed_ms") status(form, "error", T.tooFast);
            return;
          }
          status(form, "error", T.failed);
        }).catch(function () {
          status(form, "error", T.failed);
        }).then(function () { btn.disabled = false; btn.textContent = label; });
      });
    });
  });
})();
