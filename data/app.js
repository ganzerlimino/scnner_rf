// Web UI for RF-Tracker. Strings come from /locales/en.json and /locales/it.json.
const state = {
  lang: "en",
  strings: {},
  config: null,
  token: "",
};

const $ = (id) => document.getElementById(id);

function t(key) {
  return state.strings[key] || key;
}

function applyI18n() {
  document.documentElement.lang = state.lang;
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    const key = el.dataset.i18n;
    if (el.tagName === "OPTION") {
      el.textContent = t(key);
    } else if (el.children.length === 0 || el.dataset.i18nLock === "text") {
      el.textContent = t(key);
    } else if (!el.querySelector("input, select")) {
      el.textContent = t(key);
    }
  });
  document.title = t("app.title");
}

async function loadLanguage(lang) {
  const response = await fetch(`/locales/${lang}.json`, { cache: "no-store" });
  if (!response.ok) {
    return;
  }
  state.lang = lang;
  state.strings = await response.json();
  $("language").value = lang;
  applyI18n();
  renderRadios();
  refreshStatus();
  refreshPackets();
  refreshLogs();
}

function num(value, fallback) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function field(id) {
  return $(id);
}

function renderRadios() {
  const root = $("radios");
  if (!state.config) {
    root.innerHTML = "";
    return;
  }
  root.innerHTML = state.config.radios.map((radio, index) => `
    <article class="radio-card">
      <h3>${radio.id}</h3>
      <div class="radio-grid">
        <label class="check"><input type="checkbox" data-radio="${index}" data-key="enabled" ${radio.enabled ? "checked" : ""}><span>${t("radio.enabled")}</span></label>
        ${numberRow(index, "frequency_mhz", "radio.frequency", radio.frequency_mhz, "0.01")}
        ${numberRow(index, "freq_min_mhz", "radio.min", radio.freq_min_mhz, "0.01")}
        ${numberRow(index, "freq_max_mhz", "radio.max", radio.freq_max_mhz, "0.01")}
        ${numberRow(index, "bitrate_kbps", "radio.bitrate", radio.bitrate_kbps, "0.01")}
        ${numberRow(index, "deviation_khz", "radio.deviation", radio.deviation_khz, "0.01")}
        ${numberRow(index, "rx_bandwidth_khz", "radio.bandwidth", radio.rx_bandwidth_khz, "0.01")}
        <label>${t("radio.sync")}</label>
        <input data-radio="${index}" data-key="sync_word" value="${radio.sync_word}" maxlength="6">
        ${numberRow(index, "preamble_bits", "radio.preamble", radio.preamble_bits, "1")}
        <label class="check"><input type="checkbox" data-radio="${index}" data-key="variable_length" ${radio.variable_length ? "checked" : ""}><span>${t("radio.variable")}</span></label>
        ${numberRow(index, "fixed_length", "radio.fixed", radio.fixed_length, "1")}
        ${numberRow(index, "cs_pin", "radio.cs", radio.cs_pin, "1")}
        ${numberRow(index, "gdo0_pin", "radio.gdo0", radio.gdo0_pin, "1")}
        ${numberRow(index, "gdo2_pin", "radio.gdo2", radio.gdo2_pin, "1")}
      </div>
    </article>
  `).join("");
}

function numberRow(index, key, label, value, step) {
  return `<label>${t(label)}</label><input data-radio="${index}" data-key="${key}" type="number" step="${step}" value="${value}">`;
}

function fillSystemForm() {
  const config = state.config;
  if (!config) {
    return;
  }
  $("wifi-mode").value = config.wifi.mode;
  $("ap-ssid").value = config.wifi.ap_ssid;
  $("ap-password").value = config.wifi.ap_password;
  $("hostname").value = config.wifi.hostname;
  $("ap-ip").value = config.wifi.ap_ip;
  $("channel").value = config.wifi.ap_channel;
  $("max-clients").value = config.wifi.ap_max_clients;
  $("sta-ssid").value = config.wifi.sta_ssid || "";
  $("sta-password").value = config.wifi.sta_password || "";
  $("api-token").value = config.api_token || "";
  state.token = config.api_token || "";
  $("gps-enabled").checked = !!config.gps.enabled;
  $("gps-baud").value = String(config.gps.baud);
  $("oled-enabled").checked = !!config.oled.enabled;
  $("oled-addr").value = config.oled.i2c_address;
  $("refresh").value = config.oled.refresh_ms;
  $("sd-enabled").checked = !!config.storage.enabled;
  $("prefix").value = config.storage.log_prefix;
  $("crc-column").checked = !!config.storage.include_crc_column;
  $("queue").value = config.storage.queue_depth;
  $("flush").value = config.storage.flush_every;
  $("sd-cs").value = config.storage.cs_pin;
  $("serial-packets").checked = !!config.debug.serial_packets;
}

function collectConfig() {
  const config = JSON.parse(JSON.stringify(state.config));
  config.language = $("language").value;
  config.api_token = $("api-token").value.trim();
  config.wifi.mode = $("wifi-mode").value;
  config.wifi.ap_ssid = $("ap-ssid").value.trim();
  config.wifi.ap_password = $("ap-password").value;
  config.wifi.hostname = $("hostname").value.trim();
  config.wifi.ap_ip = $("ap-ip").value.trim();
  config.wifi.ap_channel = num($("channel").value, config.wifi.ap_channel);
  config.wifi.ap_max_clients = num($("max-clients").value, config.wifi.ap_max_clients);
  config.wifi.sta_ssid = $("sta-ssid").value.trim();
  config.wifi.sta_password = $("sta-password").value;
  config.gps.enabled = $("gps-enabled").checked;
  config.gps.baud = num($("gps-baud").value, config.gps.baud);
  config.oled.enabled = $("oled-enabled").checked;
  config.oled.i2c_address = num($("oled-addr").value, config.oled.i2c_address);
  config.oled.refresh_ms = num($("refresh").value, config.oled.refresh_ms);
  config.storage.enabled = $("sd-enabled").checked;
  config.storage.log_prefix = $("prefix").value.trim();
  config.storage.include_crc_column = $("crc-column").checked;
  config.storage.queue_depth = num($("queue").value, config.storage.queue_depth);
  config.storage.flush_every = num($("flush").value, config.storage.flush_every);
  config.storage.cs_pin = num($("sd-cs").value, config.storage.cs_pin);
  config.debug.serial_packets = $("serial-packets").checked;
  document.querySelectorAll("[data-radio]").forEach((input) => {
    const radio = config.radios[Number(input.dataset.radio)];
    const key = input.dataset.key;
    if (input.type === "checkbox") {
      radio[key] = input.checked;
    } else if (key === "sync_word") {
      radio[key] = input.value.trim();
    } else {
      radio[key] = String(input.value);
    }
  });
  return config;
}

function showNotice(message, isError) {
  const notice = $("notice");
  notice.hidden = false;
  notice.textContent = message;
  notice.classList.toggle("error", !!isError);
}

async function api(path, options = {}) {
  const headers = Object.assign({ "Content-Type": "application/json" }, options.headers || {});
  if (state.token) {
    headers["X-Api-Token"] = state.token;
  }
  const response = await fetch(path, Object.assign({}, options, { headers, cache: "no-store" }));
  const text = await response.text();
  let body = {};
  try {
    body = text ? JSON.parse(text) : {};
  } catch (error) {
    body = { raw: text };
  }
  if (!response.ok) {
    const code = body.error || "http_" + response.status;
    const translated = t("action." + code);
    throw new Error(translated === "action." + code ? t("action.error") + " (" + code + ")" : translated);
  }
  return body;
}

function card(label, value) {
  return `<article class="card"><span>${label}</span><strong>${value}</strong></article>`;
}

async function refreshStatus() {
  try {
    const status = await api("/api/status");
    $("version").textContent = status.version;
    const gps = status.gps.location_valid
      ? `${t("status.fix")} ${status.gps.latitude.toFixed(5)}, ${status.gps.longitude.toFixed(5)} · ${status.gps.satellites}`
      : t("status.searching");
    const radios = status.radios.map((radio) => card(
      radio.id + " MHz " + Number(radio.frequency_mhz).toFixed(2),
      `${radio.ready ? t("status.ready") : t("status.off")} · ${radio.packets} pkt · ${radio.last_rssi_dbm === null || radio.last_rssi_dbm === undefined ? "--" : radio.last_rssi_dbm + " dBm"}`
    )).join("");
    $("status").innerHTML = [
      card(t("status.uptime"), status.uptime_s + " s"),
      card(t("status.wifi"), `${status.wifi.ssid} · ${status.wifi.ip}`),
      card(t("status.clients"), status.wifi.clients),
      card(t("status.gps"), gps),
      card(t("status.sd"), status.sd.mounted ? t("status.mounted") : t("status.missing")),
      card(t("status.queue"), status.sd.queue),
      card(t("status.written"), status.sd.written),
      card(t("status.dropped"), status.sd.dropped),
      card(t("status.heap"), status.heap_free),
    ].join("") + radios;
  } catch (error) {
    // Status polling should not cover a save error.
  }
}

async function refreshPackets() {
  try {
    const body = await api("/api/packets");
    const rows = body.packets || [];
    $("packets-empty").hidden = rows.length > 0;
    $("packets").innerHTML = rows.map((row) => {
      const where = row.location_valid ? `${row.latitude.toFixed(5)}, ${row.longitude.toFixed(5)}` : "—";
      return `<tr>
        <td class="mono">${row.timestamp_utc || "—"}</td>
        <td class="mono">${row.band}</td>
        <td class="mono">${row.rssi_dbm}</td>
        <td class="mono">${row.length}</td>
        <td class="mono">${where}</td>
        <td class="hex">${row.payload_hex}</td>
      </tr>`;
    }).join("");
  } catch (error) {
    // Ignore transient poll failures.
  }
}

async function refreshLogs() {
  const list = $("logs");
  try {
    const body = await api("/api/logs");
    if (!body.mounted) {
      list.innerHTML = `<li>${t("logs.unavailable")}</li>`;
      return;
    }
    if (!body.files || body.files.length === 0) {
      list.innerHTML = `<li>${t("logs.empty")}</li>`;
      return;
    }
    list.innerHTML = body.files.map((file) => `
      <li>
        <span class="mono">${file.name} · ${file.size} B</span>
        <span>
          <a class="ghost" href="/api/logs/download?name=${encodeURIComponent(file.name)}">${t("action.download")}</a>
          <button type="button" class="ghost" data-delete="${file.name}">${t("action.delete")}</button>
        </span>
      </li>
    `).join("");
  } catch (error) {
    list.innerHTML = `<li>${t("logs.unavailable")}</li>`;
  }
}

async function loadConfig() {
  state.config = await api("/api/config");
  const lang = state.config.language === "it" ? "it" : "en";
  await loadLanguage(lang);
  fillSystemForm();
  renderRadios();
}

$("language").addEventListener("change", async () => {
  if (state.config) {
    state.config = collectConfig();
  }
  await loadLanguage($("language").value);
  fillSystemForm();
});

$("config-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const payload = collectConfig();
    state.token = payload.api_token || "";
    const result = await api("/api/config", { method: "POST", body: JSON.stringify(payload) });
    state.config = payload;
    showNotice(result.reboot_required ? t("action.reboot_required") : t("action.saved"), false);
  } catch (error) {
    showNotice(error.message, true);
  }
});

$("reboot").addEventListener("click", async () => {
  try {
    await api("/api/system/reboot", { method: "POST", body: "{}" });
    showNotice(t("action.reboot"), false);
  } catch (error) {
    showNotice(error.message, true);
  }
});

$("reload-logs").addEventListener("click", refreshLogs);
$("logs").addEventListener("click", async (event) => {
  const name = event.target.dataset.delete;
  if (!name || !window.confirm(t("action.confirm_delete"))) {
    return;
  }
  try {
    await api("/api/logs?name=" + encodeURIComponent(name), { method: "DELETE" });
    refreshLogs();
  } catch (error) {
    showNotice(error.message, true);
  }
});

loadConfig().catch((error) => showNotice(error.message, true));
setInterval(refreshStatus, 2000);
setInterval(refreshPackets, 2000);
