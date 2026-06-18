#include "http_file_server.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_http_server.h"
#include "esp_log.h"

#define HTTP_FILE_SERVER_MAX_VIRTUAL_PATH 256
#define HTTP_FILE_SERVER_MAX_FS_PATH 320
#define HTTP_FILE_SERVER_FILE_CHUNK_SIZE 4096

static const char *TAG = "http_file_server";
static http_file_server_config_t s_server_config;
static httpd_handle_t s_server;

static const char INDEX_HTML[] =
"<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>ESP32-S3 WebFS</title><style>"
":root{color-scheme:light dark;--bg:#eef2f5;--panel:#ffffff;--panel2:#f7f9fb;--text:#14212b;--muted:#667684;--line:#d7e0e7;--accent:#0a6f86;--accent2:#e6f5f7;--danger:#b42318;--shadow:0 12px 32px rgba(20,40,55,.10)}"
"*{box-sizing:border-box}html,body{height:100%;overflow:hidden}body{margin:0;font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;background:var(--bg);color:var(--text)}"
"button{height:36px;border:1px solid var(--line);border-radius:7px;background:var(--panel);color:var(--text);padding:0 12px;cursor:pointer;white-space:nowrap;font:inherit}button.primary{background:var(--accent);border-color:var(--accent);color:#fff}button.danger{color:var(--danger)}button:disabled{opacity:.48;cursor:not-allowed}"
".shell{height:100dvh;display:grid;grid-template-rows:auto 1fr auto}.top,.bottom{background:rgba(255,255,255,.92);backdrop-filter:blur(14px);border-color:var(--line);z-index:3}.top{border-bottom:1px solid var(--line)}.bottom{border-top:1px solid var(--line)}.bar,.foot{max-width:1180px;margin:0 auto;padding:12px 16px}.brand{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px}.brand h1{margin:0;font-size:18px;font-weight:750;letter-spacing:0}.usage{color:var(--muted);font-size:13px;white-space:nowrap}.nav{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:10px;align-items:center}.crumb{height:38px;display:flex;align-items:center;border:1px solid var(--line);border-radius:8px;background:var(--panel2);padding:0 11px;font-weight:650;word-break:break-all;overflow:hidden}.crumb span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.tools{display:flex;gap:8px}"
".main{min-height:0;padding:12px 16px}.viewport{height:100%;max-width:1180px;margin:0 auto;border:1px solid var(--line);border-radius:8px;background:var(--panel);box-shadow:var(--shadow);overflow:auto}.list{min-width:760px}.row{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:14px;align-items:center;min-height:58px;padding:10px 13px;border-top:1px solid var(--line)}.row:first-child{border-top:0}.row:hover{background:var(--panel2)}.head{position:sticky;top:0;z-index:2;min-height:38px;background:var(--panel2);color:var(--muted);font-size:12px;font-weight:750;text-transform:uppercase}.head:hover{background:var(--panel2)}.fileinfo{min-width:0;display:grid;grid-template-columns:minmax(220px,1fr) 82px 110px 176px;gap:12px;align-items:center}.name{display:flex;align-items:center;gap:10px;min-width:0}.badge{display:grid;place-items:center;width:34px;height:34px;border-radius:8px;background:var(--accent2);color:var(--accent);font-weight:800;font-size:11px;flex:0 0 auto}.name button{height:auto;border:0;background:transparent;padding:0;color:var(--text);font:inherit;font-weight:650;white-space:normal;text-align:left;word-break:break-word;line-height:1.25}.name button:hover{color:var(--accent)}.meta{color:var(--muted);font-size:13px}.actions{display:flex;justify-content:flex-end;gap:7px}.actions button{height:32px;padding:0 10px}.empty{padding:54px 16px;text-align:center;color:var(--muted)}"
".foot{display:grid;grid-template-columns:minmax(0,1fr) auto minmax(180px,260px) minmax(80px,150px);gap:10px;align-items:center}.picker{min-width:0;height:36px;display:flex;align-items:center;gap:10px;border:1px solid var(--line);border-radius:7px;background:var(--panel);color:var(--text);padding:0 12px;cursor:pointer}.picker input{display:none}.picked{min-width:0;color:var(--muted);font-size:13px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.progress{height:8px;background:var(--panel2);border:1px solid var(--line);border-radius:999px;overflow:hidden}.progress[hidden],#status[hidden]{display:none}.progress>i{display:block;height:100%;width:0;background:var(--accent)}#status{color:var(--muted);font-size:13px;text-align:right;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
"dialog{border:1px solid var(--line);border-radius:8px;padding:16px;background:var(--panel);color:var(--text);width:min(360px,calc(100vw - 28px));box-shadow:var(--shadow)}dialog form{display:grid;gap:12px}input[type=text]{height:38px;border:1px solid var(--line);border-radius:7px;padding:0 10px;background:var(--panel);color:var(--text)}menu{display:flex;justify-content:flex-end;gap:8px;margin:0;padding:0}"
"@media(max-width:760px){body{font-size:13px}.bar,.foot,.main{padding:10px}.brand{margin-bottom:8px}.brand h1{font-size:17px}.nav{grid-template-columns:1fr}.tools{display:grid;grid-template-columns:1fr 1fr 1fr}.tools button{width:100%;padding:0 6px}.main{padding-top:8px;padding-bottom:8px}.viewport{border-radius:8px}.list{min-width:0}.head{display:none}.row{grid-template-columns:minmax(0,1fr) auto;gap:8px;min-height:66px;padding:10px}.fileinfo{grid-template-columns:1fr;gap:3px}.name{gap:9px}.badge{width:32px;height:32px}.name button{font-size:14px}.type{display:none}.meta{font-size:12px}.actions{display:grid;grid-template-columns:1fr;gap:5px;min-width:76px}.actions button{height:30px;padding:0 8px}.foot{grid-template-columns:1fr auto;grid-template-areas:'file upload' 'progress status'}.picker{grid-area:file;padding:0 10px}.picked{flex:1}#upload{grid-area:upload}.progress{grid-area:progress}#status{grid-area:status;text-align:right}}"
"@media(prefers-color-scheme:dark){:root{--bg:#0f1418;--panel:#171d22;--panel2:#20272e;--text:#e8eef3;--muted:#9aa7b2;--line:#303a42;--accent:#68c4d8;--accent2:#183841;--shadow:none}.top,.bottom{background:rgba(23,29,34,.94)}}"
"</style></head><body><div class=\"shell\"><header class=\"top\"><div class=\"bar\"><div class=\"brand\"><h1>ESP32-S3 WebFS</h1><div class=\"usage\" id=\"usage\"></div></div><div class=\"nav\"><div class=\"crumb\"><span id=\"crumb\">/</span></div><div class=\"tools\"><button id=\"up\">&#8593; &#19978;&#32423;</button><button id=\"mkdir\" class=\"primary\">+ &#30446;&#24405;</button><button id=\"refresh\">&#8635; &#21047;&#26032;</button></div></div></div></header>"
"<main class=\"main\"><section class=\"viewport\"><div class=\"list\" id=\"list\"></div></section></main>"
"<footer class=\"bottom\"><div class=\"foot\"><label class=\"picker\"><input id=\"file\" type=\"file\" multiple=\"multiple\"><span>&#36873;&#25321;&#25991;&#20214;</span><span id=\"picked\" class=\"picked\"></span></label><button id=\"upload\" class=\"primary\">&#19978;&#20256;</button><div class=\"progress\" hidden><i id=\"prog\"></i></div><span id=\"status\" hidden></span></div></footer></div>"
"<dialog id=\"dlg\"><form method=\"dialog\"><strong id=\"dlgTitle\"></strong><input id=\"dlgInput\" type=\"text\" autocomplete=\"off\"><menu><button value=\"cancel\">&#21462;&#28040;</button><button id=\"dlgOk\" value=\"ok\" class=\"primary\">&#30830;&#23450;</button></menu></form></dialog>"
"<script>"
"const $=s=>document.querySelector(s);let cwd='/',selected=[];const enc=p=>encodeURIComponent(p);"
"const T={used:'\\u5df2\\u7528 ',empty:'\\u6b64\\u76ee\\u5f55\\u4e3a\\u7a7a',dir:'\\u76ee\\u5f55',file:'\\u6587\\u4ef6',download:'\\u4e0b\\u8f7d',rename:'\\u91cd\\u547d\\u540d',del:'\\u5220\\u9664',name:'\\u540d\\u79f0',type:'\\u7c7b\\u578b',size:'\\u5927\\u5c0f',mtime:'\\u4fee\\u6539\\u65f6\\u95f4',action:'\\u64cd\\u4f5c',mkdir:'\\u65b0\\u5efa\\u76ee\\u5f55',done:'\\u4e0a\\u4f20\\u5b8c\\u6210',root:'\\u6839\\u76ee\\u5f55'};"
"function join(a,b){return (a==='/'?'/':a+'/')+b}function parent(p){if(p==='/')return'/';let i=p.lastIndexOf('/');return i<=0?'/':p.slice(0,i)}"
"function size(n){if(n===0)return'0 B';let u=['B','KB','MB','GB'],i=0;while(n>=1024&&i<u.length-1){n/=1024;i++}return(n<10&&i?n.toFixed(1):Math.round(n))+' '+u[i]}"
"function date(t){return t?new Date(t*1000).toLocaleString():'-'}function setStatus(s){let el=$('#status');el.textContent=s||'';el.hidden=!s}"
"function esc(s){return s.replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]))}"
"function js(s){return String(s).replace(/\\\\/g,'\\\\\\\\').replace(/'/g,\"\\\\'\")}"
"async function api(url,opt){let r=await fetch(url,opt);if(!r.ok)throw new Error(await r.text()||r.status);let ct=r.headers.get('content-type')||'';return ct.includes('json')?r.json():r.text()}"
"async function load(p=cwd){let d=await api('/api/list?path='+enc(p));cwd=d.path;$('#crumb').textContent=cwd;$('#usage').textContent=T.used+size(d.used)+' / '+size(d.total);$('#up').disabled=cwd==='/';render(d.entries)}"
"function render(items){if(!items.length){$('#list').innerHTML='<div class=\"empty\">'+T.empty+'</div>';return}let head='<div class=\"row head\"><div class=\"fileinfo\"><div>'+T.name+'</div><div>'+T.type+'</div><div>'+T.size+'</div><div>'+T.mtime+'</div></div><div class=\"actions\">'+T.action+'</div></div>';let rows=items.map(e=>{let type=e.type==='dir'?T.dir:T.file,sz=e.type==='dir'?'-':size(e.size),mt=date(e.mtime);return `<div class=\"row\"><div class=\"fileinfo\"><div class=\"name\"><span class=\"badge\">${e.type==='dir'?'DIR':'FILE'}</span><button onclick=\"openItem('${js(e.path)}','${e.type}')\">${esc(e.name)}</button></div><div class=\"type meta\">${type}</div><div class=\"size meta\">${sz}</div><div class=\"mtime meta\">${mt}</div></div><div class=\"actions\">${e.type==='file'?`<button onclick=\"location.href='/api/download?path=${enc(e.path)}'\">${T.download}</button>`:''}<button onclick=\"renameItem('${js(e.path)}','${js(e.name)}')\">${T.rename}</button><button class=\"danger\" onclick=\"delItem('${js(e.path)}')\">${T.del}</button></div></div>`}).join('');$('#list').innerHTML=head+rows}"
"function openItem(p,t){if(t==='dir')load(p);else location.href='/api/download?path='+enc(p)}"
"async function promptText(title,val=''){let d=$('#dlg'),i=$('#dlgInput');$('#dlgTitle').textContent=title;i.value=val;d.showModal();i.focus();return new Promise(res=>{d.onclose=()=>res(d.returnValue==='ok'?i.value.trim():null)})}"
"async function mkdir(){let n=await promptText(T.mkdir);if(!n)return;await api('/api/mkdir?path='+enc(join(cwd,n)),{method:'POST'});load()}"
"async function renameItem(p,n){let v=await promptText(T.rename,n);if(!v||v===n)return;await api('/api/rename?from='+enc(p)+'&to='+enc(join(parent(p),v)),{method:'POST'});load()}"
"async function delItem(p){if(!confirm(T.del+' '+p+' ?'))return;await api('/api/delete?path='+enc(p),{method:'POST'});load()}"
"function setProgress(done,total){$('.progress').hidden=!total;$('#prog').style.width=total?Math.min(100,Math.round(done*100/total))+'%':'0'}"
"async function uploadOne(f,done,total,index,count){let off=0,chunk=65536,target=join(cwd,f.name);if(f.size===0){await api('/api/upload?path='+enc(target)+'&offset=0&truncate=1',{method:'POST',body:new Blob([])});setProgress(done,total);setStatus(index+'/'+count+' '+f.name+' 100%');return 0}while(off<f.size){let end=Math.min(off+chunk,f.size),body=f.slice(off,end),tries=0;for(;;){try{let r=await api('/api/upload?path='+enc(target)+'&offset='+off+(off===0?'&truncate=1':''),{method:'POST',body});off=r.offset;setProgress(done+off,total);setStatus(index+'/'+count+' '+f.name+' '+Math.round(off*100/f.size)+'%');break}catch(e){if(++tries>5)throw e;await new Promise(r=>setTimeout(r,700*tries));let s=await api('/api/stat?path='+enc(target)).catch(()=>({size:off}));off=s.size||0;setProgress(done+off,total)}}}return off}"
"function choose(input){selected=[...input.files];$('#picked').textContent=selected.length?selected.length+'\\u4e2a\\u6587\\u4ef6':'';setStatus('');setProgress(0,0)}"
"async function upload(){let files=selected;if(!files.length){setStatus('');setProgress(0,0);return}let total=files.reduce((n,f)=>n+f.size,0),done=0;$('#upload').disabled=true;setProgress(0,total);try{for(let i=0;i<files.length;i++){done+=await uploadOne(files[i],done,total,i+1,files.length)}$('#file').value='';selected=[];$('#picked').textContent='';await load();setProgress(0,0);setStatus('')}catch(e){setStatus(e.message)}finally{$('#upload').disabled=false}}"
"$('#up').onclick=()=>load(parent(cwd));$('#refresh').onclick=()=>load();$('#mkdir').onclick=mkdir;$('#upload').onclick=upload;$('#file').onchange=e=>choose(e.target);load().catch(e=>setStatus(e.message));"
"</script></body></html>";

static bool is_hex(char c)
{
    return isxdigit((unsigned char)c);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    return c - 'a' + 10;
}

static bool url_decode(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0'; ++i) {
        char c = src[i];
        if (c == '%' && is_hex(src[i + 1]) && is_hex(src[i + 2])) {
            c = (char)((hex_value(src[i + 1]) << 4) | hex_value(src[i + 2]));
            i += 2;
        } else if (c == '+') {
            c = ' ';
        }
        if (out + 1 >= dst_size) {
            return false;
        }
        dst[out++] = c;
    }
    dst[out] = '\0';
    return true;
}

static esp_err_t get_decoded_query_value(httpd_req_t *req, const char *key, char *dst, size_t dst_size)
{
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    char *query = malloc(query_len + 1);
    char encoded[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH * 3];
    if (!query) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = httpd_req_get_url_query_str(req, query, query_len + 1);
    if (err == ESP_OK) {
        err = httpd_query_key_value(query, key, encoded, sizeof(encoded));
    }
    free(query);

    if (err != ESP_OK) {
        return err;
    }
    return url_decode(dst, dst_size, encoded) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static bool normalize_virtual_path(const char *input, char *out, size_t out_size)
{
    if (!input || !out || out_size < 2) {
        return false;
    }

    char tmp[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    if (input[0] == '\0') {
        strlcpy(tmp, "/", sizeof(tmp));
    } else if (input[0] == '/') {
        strlcpy(tmp, input, sizeof(tmp));
    } else {
        snprintf(tmp, sizeof(tmp), "/%s", input);
    }

    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = '\0';
    }

    for (const char *p = tmp; *p; ++p) {
        if ((unsigned char)*p < 0x20 || *p == '\\') {
            return false;
        }
    }

    char rebuilt[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH] = "/";
    char *saveptr = NULL;
    char *segment = strtok_r(tmp + 1, "/", &saveptr);
    while (segment) {
        if (segment[0] == '\0' || strcmp(segment, ".") == 0 || strcmp(segment, "..") == 0) {
            return false;
        }
        if (strlen(rebuilt) + strlen(segment) + 2 > sizeof(rebuilt)) {
            return false;
        }
        if (strcmp(rebuilt, "/") != 0) {
            strlcat(rebuilt, "/", sizeof(rebuilt));
        }
        strlcat(rebuilt, segment, sizeof(rebuilt));
        segment = strtok_r(NULL, "/", &saveptr);
    }

    strlcpy(out, rebuilt, out_size);
    return true;
}

static bool build_fs_path(const char *virtual_path, char *fs_path, size_t fs_path_size)
{
    char clean[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    if (!normalize_virtual_path(virtual_path, clean, sizeof(clean))) {
        return false;
    }
    if (strcmp(clean, "/") == 0) {
        return snprintf(fs_path, fs_path_size, "%s", s_server_config.base_path) < fs_path_size;
    }
    return snprintf(fs_path, fs_path_size, "%s%s", s_server_config.base_path, clean) < fs_path_size;
}

static bool join_path(char *out, size_t out_size, const char *base, const char *name)
{
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    bool base_is_root = strcmp(base, "/") == 0;
    size_t need = base_len + name_len + (base_is_root ? 1 : 2);

    if (need > out_size) {
        return false;
    }

    strlcpy(out, base, out_size);
    if (!base_is_root) {
        strlcat(out, "/", out_size);
    }
    strlcat(out, name, out_size);
    return true;
}

static esp_err_t json_escape_chunk(httpd_req_t *req, const char *s)
{
    char esc[8];
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        esp_err_t err = ESP_OK;

        switch (c) {
        case '\\':
            err = httpd_resp_sendstr_chunk(req, "\\\\");
            break;
        case '"':
            err = httpd_resp_sendstr_chunk(req, "\\\"");
            break;
        case '\b':
            err = httpd_resp_sendstr_chunk(req, "\\b");
            break;
        case '\f':
            err = httpd_resp_sendstr_chunk(req, "\\f");
            break;
        case '\n':
            err = httpd_resp_sendstr_chunk(req, "\\n");
            break;
        case '\r':
            err = httpd_resp_sendstr_chunk(req, "\\r");
            break;
        case '\t':
            err = httpd_resp_sendstr_chunk(req, "\\t");
            break;
        default:
            if (c < 0x20) {
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                err = httpd_resp_sendstr_chunk(req, esc);
            } else {
                err = httpd_resp_send_chunk(req, s, 1);
            }
        }

        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static const char *mime_type_from_path(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (!strcasecmp(ext, ".html")) return "text/html";
    if (!strcasecmp(ext, ".css")) return "text/css";
    if (!strcasecmp(ext, ".js")) return "application/javascript";
    if (!strcasecmp(ext, ".json")) return "application/json";
    if (!strcasecmp(ext, ".png")) return "image/png";
    if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(ext, ".gif")) return "image/gif";
    if (!strcasecmp(ext, ".svg")) return "image/svg+xml";
    if (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".log")) return "text/plain";
    return "application/octet-stream";
}

static const char *basename_from_path(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

static esp_err_t send_error(httpd_req_t *req, httpd_err_code_t code, const char *message)
{
    return httpd_resp_send_err(req, code, message);
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stat_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];
    struct stat st;

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK ||
        !build_fs_path(virtual_path, fs_path, sizeof(fs_path)) ||
        stat(fs_path, &st) != 0) {
        return send_error(req, HTTPD_404_NOT_FOUND, "path not found");
    }

    char body[96];
    snprintf(body, sizeof(body), "{\"size\":%ld,\"mtime\":%ld}", (long)st.st_size, (long)st.st_mtime);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t list_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH] = "/";
    char clean_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];
    size_t total = 0;
    size_t used = 0;

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK) {
        strlcpy(virtual_path, "/", sizeof(virtual_path));
    }
    if (!normalize_virtual_path(virtual_path, clean_path, sizeof(clean_path)) ||
        !build_fs_path(clean_path, fs_path, sizeof(fs_path))) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    DIR *dir = opendir(fs_path);
    if (!dir) {
        return send_error(req, HTTPD_404_NOT_FOUND, "directory not found");
    }

    if (s_server_config.get_storage_info) {
        esp_err_t info_err = s_server_config.get_storage_info(s_server_config.storage_info_ctx, &total, &used);
        if (info_err != ESP_OK) {
            ESP_LOGW(TAG, "storage info unavailable: %s", esp_err_to_name(info_err));
        }
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr_chunk(req, "{\"path\":\"");
    if (err != ESP_OK) {
        goto cleanup;
    }
    err = json_escape_chunk(req, clean_path);
    if (err != ESP_OK) {
        goto cleanup;
    }

    char meta[96];
    snprintf(meta, sizeof(meta), "\",\"total\":%u,\"used\":%u,\"entries\":[", (unsigned)total, (unsigned)used);
    err = httpd_resp_sendstr_chunk(req, meta);
    if (err != ESP_OK) {
        goto cleanup;
    }

    bool first = true;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_virtual[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
        char child_fs[HTTP_FILE_SERVER_MAX_FS_PATH];
        struct stat st;

        if (!join_path(child_virtual, sizeof(child_virtual), clean_path, entry->d_name)) {
            continue;
        }
        if (!build_fs_path(child_virtual, child_fs, sizeof(child_fs)) || stat(child_fs, &st) != 0) {
            continue;
        }

        if (!first) {
            err = httpd_resp_sendstr_chunk(req, ",");
            if (err != ESP_OK) {
                goto cleanup;
            }
        }
        first = false;

        err = httpd_resp_sendstr_chunk(req, "{\"name\":\"");
        if (err != ESP_OK) {
            goto cleanup;
        }
        err = json_escape_chunk(req, entry->d_name);
        if (err != ESP_OK) {
            goto cleanup;
        }
        err = httpd_resp_sendstr_chunk(req, "\",\"path\":\"");
        if (err != ESP_OK) {
            goto cleanup;
        }
        err = json_escape_chunk(req, child_virtual);
        if (err != ESP_OK) {
            goto cleanup;
        }

        char item[128];
        snprintf(item, sizeof(item), "\",\"type\":\"%s\",\"size\":%ld,\"mtime\":%ld}",
                 S_ISDIR(st.st_mode) ? "dir" : "file", (long)st.st_size, (long)st.st_mtime);
        err = httpd_resp_sendstr_chunk(req, item);
        if (err != ESP_OK) {
            goto cleanup;
        }
    }

    err = httpd_resp_sendstr_chunk(req, "]}");
    if (err == ESP_OK) {
        err = httpd_resp_send_chunk(req, NULL, 0);
    }

cleanup:
    closedir(dir);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "list response stopped: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t download_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];
    struct stat st;

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK ||
        !build_fs_path(virtual_path, fs_path, sizeof(fs_path)) ||
        stat(fs_path, &st) != 0 ||
        S_ISDIR(st.st_mode)) {
        return send_error(req, HTTPD_404_NOT_FOUND, "file not found");
    }

    FILE *file = fopen(fs_path, "rb");
    if (!file) {
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open failed");
    }

    char disposition[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH + 64];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", basename_from_path(fs_path));
    httpd_resp_set_type(req, mime_type_from_path(fs_path));
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);

    char *buf = malloc(HTTP_FILE_SERVER_FILE_CHUNK_SIZE);
    if (!buf) {
        fclose(file);
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
    }

    size_t read_len;
    esp_err_t err = ESP_OK;
    while ((read_len = fread(buf, 1, HTTP_FILE_SERVER_FILE_CHUNK_SIZE, file)) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_len) != ESP_OK) {
            err = ESP_FAIL;
            break;
        }
    }

    free(buf);
    fclose(file);
    if (err == ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return err;
}

static esp_err_t mkdir_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK ||
        !build_fs_path(virtual_path, fs_path, sizeof(fs_path)) ||
        strcmp(fs_path, s_server_config.base_path) == 0) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    if (mkdir(fs_path, 0775) != 0 && errno != EEXIST) {
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "mkdir failed");
    }
    return httpd_resp_sendstr(req, "ok");
}

static int remove_recursive(const char *fs_path)
{
    struct stat st;
    if (stat(fs_path, &st) != 0) {
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(fs_path);
    }

    DIR *dir = opendir(fs_path);
    if (!dir) {
        return -1;
    }

    struct dirent *entry;
    int ret = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[HTTP_FILE_SERVER_MAX_FS_PATH];
        if (!join_path(child, sizeof(child), fs_path, entry->d_name) || remove_recursive(child) != 0) {
            ret = -1;
            break;
        }
    }
    closedir(dir);
    if (ret == 0) {
        ret = rmdir(fs_path);
    }
    return ret;
}

static esp_err_t delete_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK ||
        !build_fs_path(virtual_path, fs_path, sizeof(fs_path)) ||
        strcmp(fs_path, s_server_config.base_path) == 0) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    if (remove_recursive(fs_path) != 0) {
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "delete failed");
    }
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t rename_handler(httpd_req_t *req)
{
    char from_virtual[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char to_virtual[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char from_fs[HTTP_FILE_SERVER_MAX_FS_PATH];
    char to_fs[HTTP_FILE_SERVER_MAX_FS_PATH];

    if (get_decoded_query_value(req, "from", from_virtual, sizeof(from_virtual)) != ESP_OK ||
        get_decoded_query_value(req, "to", to_virtual, sizeof(to_virtual)) != ESP_OK ||
        !build_fs_path(from_virtual, from_fs, sizeof(from_fs)) ||
        !build_fs_path(to_virtual, to_fs, sizeof(to_fs)) ||
        strcmp(from_fs, s_server_config.base_path) == 0 ||
        strcmp(to_fs, s_server_config.base_path) == 0) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    if (rename(from_fs, to_fs) != 0) {
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "rename failed");
    }
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t upload_handler(httpd_req_t *req)
{
    char virtual_path[HTTP_FILE_SERVER_MAX_VIRTUAL_PATH];
    char fs_path[HTTP_FILE_SERVER_MAX_FS_PATH];
    char offset_str[32] = "0";
    char truncate_str[8] = "0";
    long offset = 0;

    if (get_decoded_query_value(req, "path", virtual_path, sizeof(virtual_path)) != ESP_OK ||
        !build_fs_path(virtual_path, fs_path, sizeof(fs_path)) ||
        strcmp(fs_path, s_server_config.base_path) == 0) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    if (get_decoded_query_value(req, "offset", offset_str, sizeof(offset_str)) == ESP_OK) {
        offset = strtol(offset_str, NULL, 10);
    }
    get_decoded_query_value(req, "truncate", truncate_str, sizeof(truncate_str));
    if (offset < 0) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "invalid offset");
    }

    struct stat st = {0};
    bool exists = stat(fs_path, &st) == 0;
    if (exists && S_ISDIR(st.st_mode)) {
        return send_error(req, HTTPD_400_BAD_REQUEST, "target is directory");
    }

    if (offset == 0 && strcmp(truncate_str, "1") == 0) {
        FILE *reset = fopen(fs_path, "wb");
        if (!reset) {
            return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "create failed");
        }
        fclose(reset);
        st.st_size = 0;
        exists = true;
    } else if (!exists) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "missing target");
    }

    if ((long)st.st_size != offset) {
        char body[64];
        snprintf(body, sizeof(body), "{\"offset\":%ld}", (long)st.st_size);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    }

    FILE *file = fopen(fs_path, "ab");
    if (!file) {
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open failed");
    }

    char *buf = malloc(HTTP_FILE_SERVER_FILE_CHUNK_SIZE);
    if (!buf) {
        fclose(file);
        return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
    }

    int remaining = req->content_len;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, remaining > HTTP_FILE_SERVER_FILE_CHUNK_SIZE ? HTTP_FILE_SERVER_FILE_CHUNK_SIZE : remaining);
        if (recv_len <= 0) {
            free(buf);
            fclose(file);
            return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
        }
        if (fwrite(buf, 1, recv_len, file) != (size_t)recv_len) {
            free(buf);
            fclose(file);
            return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
        }
        remaining -= recv_len;
        offset += recv_len;
    }

    free(buf);
    fclose(file);

    char body[64];
    snprintf(body, sizeof(body), "{\"offset\":%ld}", offset);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_file_server_start(const http_file_server_config_t *server_config)
{
    if (!server_config || !server_config->base_path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_server) {
        return ESP_ERR_INVALID_STATE;
    }
    s_server_config = *server_config;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.stack_size = 8192;
    http_config.lru_purge_enable = true;
    http_config.recv_wait_timeout = 15;
    http_config.send_wait_timeout = 15;

    esp_err_t err = httpd_start(&s_server, &http_config);
    if (err != ESP_OK) {
        return err;
    }

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    const httpd_uri_t list = {.uri = "/api/list", .method = HTTP_GET, .handler = list_handler};
    const httpd_uri_t stat = {.uri = "/api/stat", .method = HTTP_GET, .handler = stat_handler};
    const httpd_uri_t download = {.uri = "/api/download", .method = HTTP_GET, .handler = download_handler};
    const httpd_uri_t upload = {.uri = "/api/upload", .method = HTTP_POST, .handler = upload_handler};
    const httpd_uri_t mkdir_uri = {.uri = "/api/mkdir", .method = HTTP_POST, .handler = mkdir_handler};
    const httpd_uri_t delete_uri = {.uri = "/api/delete", .method = HTTP_POST, .handler = delete_handler};
    const httpd_uri_t rename_uri = {.uri = "/api/rename", .method = HTTP_POST, .handler = rename_handler};

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &stat));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &download));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &upload));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &mkdir_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &delete_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &rename_uri));

    ESP_LOGI(TAG, "HTTP file server started");
    return ESP_OK;
}

esp_err_t http_file_server_stop(void)
{
    if (!s_server) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = httpd_stop(s_server);
    if (err == ESP_OK) {
        s_server = NULL;
    }
    return err;
}

bool http_file_server_is_running(void)
{
    return s_server != NULL;
}

