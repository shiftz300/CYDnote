#ifndef AP_SHARE_SERVICE_H
#define AP_SHARE_SERVICE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "sd_helper.h"

class ApShareService {
private:
    static constexpr size_t SHARE_LIST_MAX_ENTRIES = 1200;
    static constexpr uint32_t TOGGLE_DEBOUNCE_MS = 700;
    static constexpr uint32_t WIFI_OFF_DELAY_MS = 2500;
    static constexpr size_t UPLOAD_MAX_BYTES = 512UL * 1024UL;

    StorageHelper* sd_helper;
    WebServer* server;
    DNSServer* dns;
    bool running;
    bool switching;
    uint32_t last_toggle_ms;
    bool wifi_off_pending;
    uint32_t wifi_off_due_ms;
    String ssid;

    File upload_lfs;
    FsFile upload_sd;
    char upload_drive;
    bool upload_ok;
    bool upload_failed;
    size_t upload_bytes;
    String upload_vpath;
    String upload_error;

public:
    ApShareService()
        : sd_helper(nullptr), server(nullptr), dns(nullptr), running(false), switching(false),
          last_toggle_ms(0), wifi_off_pending(false), wifi_off_due_ms(0), ssid("CYDnote-Share"),
          upload_drive('L'), upload_ok(false), upload_failed(false), upload_bytes(0),
          upload_vpath(""), upload_error("") {}

    void init(StorageHelper* helper) { sd_helper = helper; }

    void update() {
        if (!running && wifi_off_pending) {
            uint32_t now = millis();
            if ((int32_t)(now - wifi_off_due_ms) >= 0) {
                WiFi.mode(WIFI_OFF);
                wifi_off_pending = false;
            }
        }
        if (running && dns) dns->processNextRequest();
        if (running && server) server->handleClient();
    }

    bool toggle() {
        uint32_t now = millis();
        if (switching) return running;
        if ((uint32_t)(now - last_toggle_ms) < TOGGLE_DEBOUNCE_MS) return running;
        switching = true;
        last_toggle_ms = now;
        bool out = false;
        if (running) {
            stop();
            out = false;
        } else {
            out = start();
        }
        switching = false;
        last_toggle_ms = millis();
        return out;
    }

    bool isRunning() const { return running; }

    String statusString() const {
        if (!running) return "Status: Stopped";
        IPAddress ip = WiFi.softAPIP();
        String s = "Status: Running\nSSID: " + ssid;
        s += "\nURL: http://" + ip.toString() + "/";
        return s;
    }

private:
    bool isDriveReady(char d) const {
        if (d == 'L') return true;
        if (d == 'D') return sd_helper && sd_helper->isInitialized();
        return false;
    }

    static bool isAllowedTextFile(const String& p) {
        int dot = p.lastIndexOf('.');
        if (dot < 0) return false;
        String ext = p.substring(dot + 1);
        ext.toLowerCase();
        return (ext == "txt" || ext == "md" || ext == "csv" || ext == "log" || ext == "json");
    }

    static String htmlEscape(const String& s) {
        String out;
        out.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); ++i) {
            char c = s.charAt((int)i);
            if (c == '&') out += "&amp;";
            else if (c == '<') out += "&lt;";
            else if (c == '>') out += "&gt;";
            else if (c == '"') out += "&quot;";
            else out += c;
        }
        return out;
    }

    static String urlEncodeUtf8(const String& s) {
        static const char* HEX_CHARS = "0123456789ABCDEF";
        String out;
        out.reserve(s.length() * 3);
        for (size_t i = 0; i < s.length(); ++i) {
            uint8_t c = (uint8_t)s.charAt((int)i);
            bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
            if (safe) out += (char)c;
            else {
                out += '%';
                out += HEX_CHARS[(c >> 4) & 0x0F];
                out += HEX_CHARS[c & 0x0F];
            }
        }
        return out;
    }

    static String asciiFallbackName(const String& s) {
        String out;
        out.reserve(s.length());
        for (size_t i = 0; i < s.length(); ++i) {
            uint8_t c = (uint8_t)s.charAt((int)i);
            if (c >= 0x20 && c <= 0x7E && c != '"' && c != '\\') out += (char)c;
            else out += '_';
        }
        if (out.length() == 0) out = "download.txt";
        return out;
    }

    static String basenameOf(String name) {
        int slash = name.lastIndexOf('/');
        if (slash >= 0 && slash < (int)name.length() - 1) return name.substring(slash + 1);
        return name;
    }

    static String fileNameOf(const String& path) {
        int idx = path.lastIndexOf('/');
        if (idx >= 0 && idx < (int)path.length() - 1) return path.substring(idx + 1);
        return path;
    }

    static String joinPath(const String& base, const String& child) {
        if (base == "/") return "/" + child;
        return base + "/" + child;
    }

    static String innerFromVPath(const String& vpath) {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            return p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        return p;
    }

    static char driveFromVPath(const String& vpath) {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            char d = vpath.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
            return d;
        }
        return 'L';
    }

    static String buildVPath(char d, const String& inner) {
        String p = inner;
        if (!p.startsWith("/")) p = "/" + p;
        return String(d) + ":" + p;
    }

    void ensureLittleFsParent(const String& p) {
        int idx = p.lastIndexOf('/');
        if (idx <= 0) return;
        String parent = p.substring(0, idx);
        if (parent.length() == 0) return;
        String cur = "";
        int start = 1;
        while (start >= 0 && start < (int)parent.length()) {
            int slash = parent.indexOf('/', start);
            String seg = (slash < 0) ? parent.substring(start) : parent.substring(start, slash);
            if (seg.length() > 0) {
                cur += "/" + seg;
                if (!LittleFS.exists(cur.c_str())) LittleFS.mkdir(cur.c_str());
            }
            if (slash < 0) break;
            start = slash + 1;
        }
    }

    void ensureSdParent(const String& p) {
        if (!sd_helper || !sd_helper->isInitialized()) return;
        int idx = p.lastIndexOf('/');
        if (idx <= 0) return;
        String parent = p.substring(0, idx);
        if (parent.length() > 0) sd_helper->getFs().mkdir(parent.c_str(), true);
    }

    bool parsePathArg(String path_arg, String drive_arg, bool allow_empty, String& out_vpath, String& err) const {
        path_arg = WebServer::urlDecode(path_arg);
        drive_arg.trim();
        if (drive_arg.length() > 0) drive_arg = drive_arg.substring(0, 1);
        char d = 'L';
        if (drive_arg.length() > 0) {
            d = drive_arg.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
        }
        String p = path_arg;
        p.trim();
        p.replace("\\", "/");
        if (p.length() >= 2 && p.charAt(1) == ':') {
            d = p.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
            p = p.substring(2);
        }
        if (!isDriveReady(d)) {
            err = (d == 'D') ? "SD is not mounted" : "invalid drive";
            return false;
        }
        if (p.length() == 0) p = allow_empty ? "/" : "/upload.txt";
        if (!p.startsWith("/")) p = "/" + p;
        while (p.indexOf("//") >= 0) p.replace("//", "/");
        if (p.indexOf("..") >= 0) { err = "invalid path"; return false; }
        if (!allow_empty && p.endsWith("/")) { err = "invalid path"; return false; }
        if (allow_empty && p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
        out_vpath = buildVPath(d, p);
        return true;
    }

    void closeUploadHandles() {
        if (upload_lfs) upload_lfs.close();
        if (upload_sd.isOpen()) upload_sd.close();
    }

    bool removeVPath(const String& vpath) {
        char d = driveFromVPath(vpath);
        String p = innerFromVPath(vpath);
        if (d == 'L') return LittleFS.remove(p.c_str());
        if (d == 'D' && sd_helper && sd_helper->isInitialized()) return sd_helper->getFs().remove(p.c_str());
        return false;
    }

    String buildPage(const String& msg = "") const {
        String html;
        html.reserve(2800);
        html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>CYDnote Share</title><style>body{font-family:sans-serif;background:#111;color:#eee;margin:14px}a{color:#8ed1ff}input,button,select{padding:8px;margin:4px 0;background:#222;color:#eee;border:1px solid #444;border-radius:6px}section{border:1px solid #333;border-radius:8px;padding:10px;margin:10px 0}.ok{color:#9ff7b5}.err{color:#ff9ea2}ul{margin:6px 0 0 16px;padding:0}li{margin:2px 0}.dim{color:#888}</style></head><body>";
        html += "<h3>CYDnote Hotspot Transfer</h3>";
        html += "<div><b>Text files only</b> (.txt/.md/.csv/.log/.json), max upload 512KB</div>";
        if (msg.length()) html += "<div class='" + String(msg.startsWith("OK:") ? "ok" : "err") + "'>" + htmlEscape(msg) + "</div>";
        html += "<section><h4>Upload</h4><form method='POST' action='/upload' enctype='multipart/form-data'><div>Drive: <select name='drive'><option value='L'>L:/</option><option value='D'>D:/</option></select></div><div>Path: <input name='path' placeholder='/notes/test.txt'></div><input type='file' name='file' accept='.txt,.md,.csv,.log,.json'><br><button type='submit'>Upload</button></form></section>";
        html += "<section><h4>Download</h4><form method='GET' action='/download'><div>Drive: <select name='drive'><option value='L'>L:/</option><option value='D'>D:/</option></select></div><div>Path: <input name='path' placeholder='/readme.md'></div><button type='submit'>Download</button></form></section>";
        html += "<section><h4>List</h4><form method='GET' action='/'><button type='submit'>Refresh</button></form><ul><li><details class='dir' data-drive='L' data-path='/' open><summary>L:/</summary><ul class='children'></ul></details></li><li><details class='dir' data-drive='D' data-path='/' open><summary>D:/</summary><ul class='children'></ul></details></li></ul>";
        html += "<script>async function L(d){const u=d.querySelector('ul.children');if(!u||d.dataset.loaded==='1')return;u.innerHTML='<li class=\"dim\">loading...</li>';const r=await fetch('/list?drive='+encodeURIComponent(d.dataset.drive)+'&path='+encodeURIComponent(d.dataset.path));u.innerHTML=r.ok?await r.text():'<li class=\"err\">load failed</li>';d.dataset.loaded='1';B(u);}function B(root){(root||document).querySelectorAll('details.dir').forEach(d=>{if(d.dataset.bound==='1')return;d.dataset.bound='1';d.addEventListener('toggle',()=>{if(d.open)L(d);});if(d.open)L(d);});}B(document);</script></section></body></html>";
        return html;
    }

    bool start() {
        if (running) return true;
        wifi_off_pending = false;
        wifi_mode_t mode = WiFi.getMode();
        if (mode != WIFI_AP && mode != WIFI_AP_STA) WiFi.mode(WIFI_AP);
        if (!WiFi.softAP(ssid.c_str())) return false;
        IPAddress ip = WiFi.softAPIP();
        if (!dns) dns = new DNSServer();
        if (dns) dns->start(53, "*", ip);
        if (!server) server = new WebServer(80);
        if (!server) return false;

        server->on("/", HTTP_GET, [this]() { server->send(200, "text/html; charset=utf-8", buildPage()); });
        server->on("/list", HTTP_GET, [this]() {
            String vpath, err;
            if (!parsePathArg(server->arg("path"), server->arg("drive"), true, vpath, err)) return server->send(400, "text/html; charset=utf-8", "<li class='err'>Invalid</li>");
            char d = driveFromVPath(vpath);
            String dir = innerFromVPath(vpath);
            String out;
            size_t count = 0;
            if (d == 'L') {
                File root = LittleFS.open(dir.c_str(), "r");
                if (!root || !root.isDirectory()) return server->send(404, "text/html; charset=utf-8", "<li class='err'>not found</li>");
                while (true) {
                    File e = root.openNextFile(); if (!e) break;
                    String name = basenameOf(String(e.name())); bool is_dir = e.isDirectory(); e.close();
                    if (!name.length()) continue;
                    String full = joinPath(dir, name);
                    if (is_dir) out += "<li><details class='dir' data-drive='L' data-path='" + htmlEscape(full) + "'><summary>" + htmlEscape(name) + "</summary><ul class='children'></ul></details></li>";
                    else if (isAllowedTextFile(name)) out += "<li><a href='/download?drive=L&path=" + urlEncodeUtf8(full) + "'>" + htmlEscape(name) + "</a></li>";
                    else out += "<li class='dim'>" + htmlEscape(name) + "</li>";
                    if (++count > SHARE_LIST_MAX_ENTRIES) return server->send(200, "text/html; charset=utf-8", "<li class='err'>Directory too large</li>");
                }
                root.close();
            } else if (d == 'D' && sd_helper && sd_helper->isInitialized()) {
                FsFile root = sd_helper->getFs().open(dir.c_str(), O_RDONLY);
                if (!root.isOpen() || !root.isDir()) return server->send(404, "text/html; charset=utf-8", "<li class='err'>not found</li>");
                FsFile e;
                while (e.openNext(&root, O_RDONLY)) {
                    char buf[256]; buf[0] = '\0'; e.getName(buf, sizeof(buf)); String name = basenameOf(String(buf)); bool is_dir = e.isDir(); e.close();
                    if (!name.length()) continue;
                    String full = joinPath(dir, name);
                    if (is_dir) out += "<li><details class='dir' data-drive='D' data-path='" + htmlEscape(full) + "'><summary>" + htmlEscape(name) + "</summary><ul class='children'></ul></details></li>";
                    else if (isAllowedTextFile(name)) out += "<li><a href='/download?drive=D&path=" + urlEncodeUtf8(full) + "'>" + htmlEscape(name) + "</a></li>";
                    else out += "<li class='dim'>" + htmlEscape(name) + "</li>";
                    if (++count > SHARE_LIST_MAX_ENTRIES) return server->send(200, "text/html; charset=utf-8", "<li class='err'>Directory too large</li>");
                }
                root.close();
            } else return server->send(404, "text/html; charset=utf-8", "<li class='err'>drive unavailable</li>");
            if (!out.length()) out = "<li class='dim'>(empty)</li>";
            server->send(200, "text/html; charset=utf-8", out);
        });

        server->on("/download", HTTP_GET, [this]() {
            String vpath, err;
            if (!parsePathArg(server->arg("path"), server->arg("drive"), false, vpath, err)) return server->send(400, "text/plain", "invalid request");
            char d = driveFromVPath(vpath);
            String p = innerFromVPath(vpath);
            String fn = fileNameOf(p);
            server->sendHeader("Content-Disposition", "attachment; filename=\"" + asciiFallbackName(fn) + "\"; filename*=UTF-8''" + urlEncodeUtf8(fn));
            if (d == 'L') {
                if (!LittleFS.exists(p.c_str())) return server->send(404, "text/plain", "file not found");
                File f = LittleFS.open(p.c_str(), "r"); if (!f) return server->send(500, "text/plain", "open failed");
                server->streamFile(f, "text/plain; charset=utf-8"); f.close(); return;
            }
            if (d == 'D' && sd_helper && sd_helper->isInitialized()) {
                FsFile f = sd_helper->getFs().open(p.c_str(), O_RDONLY); if (!f.isOpen()) return server->send(404, "text/plain", "file not found");
                server->setContentLength((size_t)f.fileSize()); server->send(200, "text/plain; charset=utf-8", " ");
                char chunk[1024]; while (true) { int n = f.read(chunk, sizeof(chunk)); if (n <= 0) break; server->sendContent(chunk, (size_t)n); } f.close(); return;
            }
            server->send(500, "text/plain", "drive unavailable");
        });

        server->on("/upload", HTTP_POST, [this]() {
            String msg = upload_ok ? "OK: upload completed" : ("ERR: " + (upload_error.length() ? upload_error : String("upload failed")));
            server->send(upload_ok ? 200 : 400, "text/html; charset=utf-8", buildPage(msg));
        }, [this]() {
            HTTPUpload& up = server->upload();
            if (up.status == UPLOAD_FILE_START) {
                closeUploadHandles(); upload_ok = false; upload_failed = false; upload_bytes = 0; upload_vpath = ""; upload_error = "";
                String req = server->hasArg("path") ? server->arg("path") : "";
                String drv = server->hasArg("drive") ? server->arg("drive") : "";
                String vpath, err;
                if (!parsePathArg(req.length() ? req : String(up.filename), drv, false, vpath, err) || !isAllowedTextFile(innerFromVPath(vpath))) { upload_failed = true; upload_error = "invalid path"; return; }
                upload_vpath = vpath; upload_drive = driveFromVPath(vpath);
                String p = innerFromVPath(vpath);
                if (upload_drive == 'L') { ensureLittleFsParent(p); upload_lfs = LittleFS.open(p.c_str(), "w"); upload_ok = (bool)upload_lfs; }
                else if (upload_drive == 'D' && sd_helper && sd_helper->isInitialized()) { ensureSdParent(p); upload_sd = sd_helper->getFs().open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC); upload_ok = upload_sd.isOpen(); }
            } else if (up.status == UPLOAD_FILE_WRITE) {
                if (!upload_ok || upload_failed) return;
                upload_bytes += up.currentSize;
                if (upload_bytes > UPLOAD_MAX_BYTES) { upload_ok = false; upload_failed = true; upload_error = "file too large"; closeUploadHandles(); if (upload_vpath.length()) removeVPath(upload_vpath); return; }
                size_t w = 0;
                if (upload_drive == 'L' && upload_lfs) w = upload_lfs.write(up.buf, up.currentSize);
                else if (upload_drive == 'D' && upload_sd.isOpen()) w = upload_sd.write(up.buf, up.currentSize);
                if (w != up.currentSize) { upload_ok = false; upload_failed = true; upload_error = "write failed"; closeUploadHandles(); if (upload_vpath.length()) removeVPath(upload_vpath); }
            } else if (up.status == UPLOAD_FILE_END) {
                closeUploadHandles();
                if (!upload_failed) upload_ok = true;
            } else if (up.status == UPLOAD_FILE_ABORTED) {
                closeUploadHandles();
                if (upload_vpath.length()) removeVPath(upload_vpath);
                upload_ok = false; upload_failed = true; upload_error = "upload aborted";
            }
        });

        auto redirect = [this]() { server->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true); server->send(302, "text/plain", "redirect"); };
        server->on("/generate_204", HTTP_GET, redirect);
        server->on("/hotspot-detect.html", HTTP_GET, redirect);
        server->on("/fwlink", HTTP_GET, redirect);
        server->on("/connecttest.txt", HTTP_GET, redirect);
        server->on("/ncsi.txt", HTTP_GET, redirect);
        server->on("/favicon.ico", HTTP_GET, [this]() { server->send(204, "text/plain", "ok"); });
        server->onNotFound(redirect);
        server->begin();
        running = true;
        return true;
    }

    void stop() {
        if (dns) { dns->stop(); delete dns; dns = nullptr; }
        if (server) { server->stop(); delete server; server = nullptr; }
        closeUploadHandles();
        upload_vpath = ""; upload_ok = false; upload_failed = false; upload_bytes = 0; upload_error = "";
        WiFi.softAPdisconnect(true);
        running = false;
        wifi_off_pending = true;
        wifi_off_due_ms = millis() + WIFI_OFF_DELAY_MS;
    }
};

#endif
