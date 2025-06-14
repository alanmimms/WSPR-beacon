#include "WebServer.h"
#include "ConfigManager.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>
#include <string>

static const char* TAG = "WebServer";
static httpd_handle_t server = NULL;

// --- HTML & JS as Raw String Literals ---

// Common navigation menu
const char* NAV_MENU_HTML = R"rawliteral(
<div class="nav">
    <a href="/">Control & Schedule</a>
    <a href="/status">Status & Stats</a>
</div>
)rawliteral";

// Main Control Page
const char* MAIN_PAGE_HTML = R"rawliteral(
<!DOCTYPE html><html><head><title>WSPR Beacon</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:sans-serif;background-color:#f4f4f4;color:#333;}
 .container{max-width:800px;margin:auto;padding:20px;background-color:#fff;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
 .nav{background-color:#333;overflow:hidden;margin-bottom:20px;border-radius:5px;}
 .nav a{float:left;color:#f2f2f2;text-align:center;padding:14px 16px;text-decoration:none;font-size:17px;}
 .nav a:hover{background-color:#ddd;color:black;}
 h1,h2{color:#0056b3;}
 label{display:block;margin-top:10px;font-weight:bold;}
 input[type=text],input[type=number],textarea,select{width:100%;padding:8px;margin-top:5px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;}
 button{background-color:#0056b3;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;}
 button:hover{background-color:#004494;}
 .status-box{border:1px solid #ddd;padding:15px;margin-top:20px;border-radius:5px;background-color:#eef;}
 .schedule-entry{border:1px solid #eee;padding:10px;margin-top:10px;border-radius:4px;}
 .days-of-week label{display:inline-block;margin-right:10px;}
</style>
</head><body><div class="container">
<h1>WSPR Beacon Control</h1>
<div class="nav"><a href="/">Control & Schedule</a><a href="/status">Status & Stats</a></div>
<div class="status-box"><strong>Current Status:</strong> <span id="opStatus">UNKNOWN</span></div>
<button id="startBtn">Start Beacon</button> <button id="stopBtn">Stop Beacon</button>
<hr>
<form id="configForm">
 <h2>Beacon Identity</h2>
 <label>Callsign:</label><input type="text" id="callsign" maxlength="11">
 <label>Grid Locator (6 char):</label><input type="text" id="locator" maxlength="6">
 <label>Power (dBm):</label><input type="number" id="power" min="0" max="60">
 <h2>Master Schedule</h2>
 <label>TX every Nth interval (0=every 2min, 1=every 4min...):</label><input type="number" id="skip" min="0">
 <label>Time Zone:</label><input type="text" id="timezone" placeholder="e.g. GMT0, EST5EDT, CST6CDT">
 
 <!-- Time Schedules -->
 <h2>Active Time Windows</h2>
 <div id="timeSchedules"></div>

 <!-- Band Plan -->
 <h2>Band Plan</h2>
 <div id="bandPlan"></div>
 <button type="button" onclick="addBand()">Add Band</button>
 <br><button type="submit">Save All Settings</button>
</form>
</div>
<script>
let configData={};
function renderPage(){
    document.getElementById('opStatus').textContent=configData.isRunning?'Running':'Stopped';
    document.getElementById('callsign').value=configData.callsign;
    document.getElementById('locator').value=configData.locator;
    document.getElementById('power').value=configData.powerDbm;
    document.getElementById('skip').value=configData.skipIntervals;
    document.getElementById('timezone').value=configData.timeZone;
    renderSchedules();
    renderBandPlan();
}
function renderSchedules(){
    let html='';
    for(let i=0;i<5;i++){
        let s=configData.timeSchedules[i];
        html+=`<div class="schedule-entry">
            <h4>Window ${i+1}</h4>
            <input type="checkbox" id="en${i}" ${s.enabled?'checked':''}> Enable<br>
            Start: <input type="number" id="sh${i}" min="0" max="23" value="${s.startHour}">:<input type="number" id="sm${i}" min="0" max="59" value="${s.startMinute}"><br>
            End: <input type="number" id="eh${i}" min="0" max="23" value="${s.endHour}">:<input type="number" id="em${i}" min="0" max="59" value="${s.endMinute}"><br>
            <div class="days-of-week">
             ${['Sun','Mon','Tue','Wed','Thu','Fri','Sat'].map((d,idx)=>`<label><input type="checkbox" class="dow${i}" value="${1<<idx}" ${s.daysOfWeek&(1<<idx)?'checked':''}>${d}</label>`).join('')}
            </div>
        </div>`;
    }
    document.getElementById('timeSchedules').innerHTML=html;
}
function renderBandPlan(){
    let html='';
    for(let i=0;i<configData.numBandsInPlan;i++){
        let b=configData.bandPlan[i];
        html+=`<div class="schedule-entry">
         Freq (Hz): <input type="number" id="freq${i}" value="${b.frequencyHz}"> 
         Iterations: <input type="number" id="iter${i}" value="${b.iterations}" min="1">
        </div>`;
    }
    document.getElementById('bandPlan').innerHTML=html;
}
function addBand(){
    if(configData.numBandsInPlan<5){
        configData.bandPlan[configData.numBandsInPlan]={frequencyHz:14097100, iterations:1};
        configData.numBandsInPlan++;
        renderBandPlan();
    }
}
document.getElementById('configForm').onsubmit=e=>{
    e.preventDefault();
    let data={
        callsign:document.getElementById('callsign').value,
        locator:document.getElementById('locator').value,
        powerDbm:parseInt(document.getElementById('power').value),
        skipIntervals:parseInt(document.getElementById('skip').value),
        timeZone:document.getElementById('timezone').value,
        timeSchedules:[],
        bandPlan:[]
    };
    for(let i=0;i<5;i++){
        let days=0;
        document.querySelectorAll('.dow${i}:checked').forEach(c=>days|=parseInt(c.value));
        data.timeSchedules.push({
            enabled:document.getElementById(`en${i}`).checked,
            startHour:parseInt(document.getElementById(`sh${i}`).value),
            startMinute:parseInt(document.getElementById(`sm${i}`).value),
            endHour:parseInt(document.getElementById(`eh${i}`).value),
            endMinute:parseInt(document.getElementById(`em${i}`).value),
            daysOfWeek:days
        });
    }
    for(let i=0;i<configData.numBandsInPlan;i++){
        data.bandPlan.push({
            frequencyHz:parseInt(document.getElementById(`freq${i}`).value),
            iterations:parseInt(document.getElementById(`iter${i}`).value)
        });
    }
    fetch('/save-config',{method:'POST',body:JSON.stringify(data)})
    .then(r=>r.text()).then(t=>{alert(t); location.reload();});
};
document.getElementById('startBtn').onclick=()=>fetch('/control',{method:'POST',body:'start'}).then(()=>location.reload());
document.getElementById('stopBtn').onclick=()=>fetch('/control',{method:'POST',body:'stop'}).then(()=>location.reload());
fetch('/api/status').then(r=>r.json()).then(d=>{configData=d;renderPage();});
</script>
</body></html>
)rawliteral";

// Status Page
const char* STATUS_PAGE_HTML = R"rawliteral(
<!DOCTYPE html><html><head><title>Beacon Status</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:sans-serif;background-color:#f4f4f4;color:#333;}
 .container{max-width:800px;margin:auto;padding:20px;background-color:#fff;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
 .nav{background-color:#333;overflow:hidden;margin-bottom:20px;border-radius:5px;}
 .nav a{float:left;color:#f2f2f2;text-align:center;padding:14px 16px;text-decoration:none;font-size:17px;}
 .nav a:hover{background-color:#ddd;color:black;}
 h1,h2{color:#0056b3;}
 table{width:100%;border-collapse:collapse;margin-top:20px;}
 th,td{border:1px solid #ddd;padding:8px;text-align:left;}
 th{background-color:#0056b3;color:white;}
</style>
</head><body><div class="container">
<h1>Beacon Status & Statistics</h1>
<div class="nav"><a href="/">Control & Schedule</a><a href="/status">Status & Stats</a></div>
<h2>Statistics</h2>
<table id="statsTable">
 <thead><tr><th>Band (Center Freq)</th><th>Total TX Minutes</th></tr></thead>
 <tbody></tbody>
</table>
</div>
<script>
fetch('/api/stats').then(r=>r.json()).then(data=>{
    const tableBody = document.querySelector('#statsTable tbody');
    tableBody.innerHTML = '';
    data.bands.forEach((band, index) => {
        const row = `<tr><td>${band.freq} Hz</td><td>${data.stats.txMinutes[index]}</td></tr>`;
        tableBody.innerHTML += row;
    });
});
</script>
</body></html>
)rawliteral";

// Provisioning Page
const char* PROV_PAGE_HTML = R"rawliteral(
<!DOCTYPE html><html><head><title>Beacon Wi-Fi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:sans-serif;text-align:center;padding-top:50px;}
 .form-container{max-width:400px;margin:auto;padding:20px;border:1px solid #ccc;border-radius:8px;}
 input[type=text],input[type=password]{width:90%;padding:10px;margin:10px 0;}
 button{padding:10px 20px;}
</style>
</head><body><div class="form-container">
<h2>Wi-Fi Beacon Setup</h2>
<form action="/save-wifi" method="post">
 <input type="text" name="ssid" placeholder="Wi-Fi SSID" required><br>
 <input type="password" name="pass" placeholder="Password"><br>
 <input type="text" name="host" placeholder="Hostname (e.g., wspr-beacon)" required><br>
 <button type="submit">Save & Reboot</button>
</form>
</div></body></html>
)rawliteral";


// --- HTTP Server Handlers ---

static esp_err_t mainPageHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, MAIN_PAGE_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t statusPageHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, STATUS_PAGE_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t provPageHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PROV_PAGE_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t statusApiHandler(httpd_req_t *req) {
    BeaconConfig config;
    ConfigManager::loadConfig(config);
    
    cJSON *root = cJSON_CreateObject();
    // ... code to populate cJSON object from config struct ...
    // This is a complex but necessary step
    
    char *jsonStr = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonStr, HTTPD_RESP_USE_STRLEN);
    free(jsonStr);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t saveConfigHandler(httpd_req_t *req) {
    char buf[2048] = {0}; // Zero-initialize the buffer
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_400(req, "Request too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req, "Timeout");
        }
        return ESP_FAIL;
    }
    // No need to null-terminate buf[ret] since the whole buffer is zeroed.
    
    cJSON *root = cJSON_Parse(buf);
    if(root == NULL) { 
        httpd_resp_send_400(req, "Invalid JSON");
        return ESP_FAIL;
    }

    BeaconConfig config;
    // We must load existing config first to not overwrite Wi-Fi settings
    ConfigManager::loadConfig(config);

    strcpy(config.callsign, cJSON_GetObjectItem(root, "callsign")->valuestring);
    strcpy(config.locator, cJSON_GetObjectItem(root, "locator")->valuestring);
    config.powerDbm = cJSON_GetObjectItem(root, "powerDbm")->valueint;
    config.skipIntervals = cJSON_GetObjectItem(root, "skipIntervals")->valueint;
    strcpy(config.timeZone, cJSON_GetObjectItem(root, "timeZone")->valuestring);

    cJSON* schedules = cJSON_GetObjectItem(root, "timeSchedules");
    int i = 0;
    cJSON* s;
    cJSON_ArrayForEach(s, schedules) {
        if(i >= 5) break;
        config.timeSchedules[i].enabled = cJSON_GetObjectItem(s, "enabled")->valueint;
        config.timeSchedules[i].startHour = cJSON_GetObjectItem(s, "startHour")->valueint;
        config.timeSchedules[i].startMinute = cJSON_GetObjectItem(s, "startMinute")->valueint;
        config.timeSchedules[i].endHour = cJSON_GetObjectItem(s, "endHour")->valueint;
        config.timeSchedules[i].endMinute = cJSON_GetObjectItem(s, "endMinute")->valueint;
        config.timeSchedules[i].daysOfWeek = cJSON_GetObjectItem(s, "daysOfWeek")->valueint;
        i++;
    }

    cJSON* bands = cJSON_GetObjectItem(root, "bandPlan");
    i=0;
    cJSON* b;
    config.numBandsInPlan = cJSON_GetArraySize(bands);
    cJSON_ArrayForEach(b, bands) {
        if(i >= 5) break;
        config.bandPlan[i].frequencyHz = cJSON_GetObjectItem(b, "frequencyHz")->valuedouble;
        config.bandPlan[i].iterations = cJSON_GetObjectItem(b, "iterations")->valueint;
        i++;
    }
    
    ConfigManager::saveConfig(config);
    
    cJSON_Delete(root);
    httpd_resp_send(req, "Configuration Saved.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t controlHandler(httpd_req_t *req) {
    char buf[10] = {0}; // Zero-initialize
    httpd_req_recv(req, buf, sizeof(buf) -1);
    BeaconConfig config;
    ConfigManager::loadConfig(config);
    if (strncmp(buf, "start", 5) == 0) {
        config.isRunning = true;
    } else {
        config.isRunning = false;
    }
    ConfigManager::saveConfig(config);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t saveWifiHandler(httpd_req_t *req) {
    char buf[256] = {0}; // Zero-initialize
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        // Simple parsing for now
        // ... parse ssid, pass, host ...
        // ... save to NVS ...
    }
    httpd_resp_send(req, "Wi-Fi Settings Saved. Rebooting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}


void WebServer::start(bool isProvisioning) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        if (isProvisioning) {
            httpd_uri_t prov_uri = { "/", HTTP_GET, provPageHandler, NULL };
            httpd_register_uri_handler(server, &prov_uri);
            httpd_uri_t save_wifi_uri = { "/save-wifi", HTTP_POST, saveWifiHandler, NULL };
            httpd_register_uri_handler(server, &save_wifi_uri);
        } else {
            httpd_uri_t main_uri = { "/", HTTP_GET, mainPageHandler, NULL };
            httpd_register_uri_handler(server, &main_uri);
            httpd_uri_t status_uri = { "/status", HTTP_GET, statusPageHandler, NULL };
            httpd_register_uri_handler(server, &status_uri);
            httpd_uri_t api_status_uri = { "/api/status", HTTP_GET, statusApiHandler, NULL };
            httpd_register_uri_handler(server, &api_status_uri);
            httpd_uri_t save_config_uri = { "/save-config", HTTP_POST, saveConfigHandler, NULL };
            httpd_register_uri_handler(server, &save_config_uri);
            httpd_uri_t control_uri = { "/control", HTTP_POST, controlHandler, NULL };
            httpd_register_uri_handler(server, &control_uri);
        }
    }
}

void WebServer::stop() {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}
