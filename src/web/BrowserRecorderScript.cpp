#include "BrowserRecorderScript.h"

namespace autopilot::web {

namespace {

constexpr const char* kTemplate = R"JS(
(function () {
  if (window.__autopilot_attached__) return;
  window.__autopilot_attached__ = true;

  var sink = window["{{BINDING_NAME}}"];
  if (typeof sink !== "function") return;

  function emit(payload) {
    try { sink(JSON.stringify(payload)); } catch (e) { /* swallow */ }
  }

  function key(type) {
    return function (e) {
      emit({
        kind: type,
        code: e.code,
        key: e.key,
        keyCode: e.keyCode,
        ctrl: e.ctrlKey, shift: e.shiftKey, alt: e.altKey, meta: e.metaKey,
        ts: performance.now()
      });
    };
  }

  function mouse(type) {
    return function (e) {
      emit({
        kind: type,
        x: e.clientX, y: e.clientY,
        button: e.button,
        ts: performance.now()
      });
    };
  }

  window.addEventListener("keydown",   key("keydown"),   true);
  window.addEventListener("keyup",     key("keyup"),     true);
  window.addEventListener("mousedown", mouse("mousedown"), true);
  window.addEventListener("mouseup",   mouse("mouseup"),   true);
  window.addEventListener("click",     mouse("click"),     true);
})();
)JS";

std::string replaceAll(std::string s, const std::string& needle, const std::string& repl) {
    std::size_t pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos) {
        s.replace(pos, needle.size(), repl);
        pos += repl.size();
    }
    return s;
}

}  // namespace

std::string BrowserRecorderScript::source(const std::string& bindingName) {
    return replaceAll(kTemplate, "{{BINDING_NAME}}", bindingName);
}

}  // namespace autopilot::web
