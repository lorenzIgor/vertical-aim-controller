#include "config.h"

#include <windows.h>

#include <charconv>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>

namespace config {
namespace {

constexpr wchar_t kFileName[] = L"vac.ini";

std::string Trim(std::string_view sv) {
    const auto notSpace = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r'; };
    while (!sv.empty() && !notSpace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && !notSpace(static_cast<unsigned char>(sv.back())))  sv.remove_suffix(1);
    return std::string(sv);
}

bool ParseFloat(const std::string& text, float& out) {
    try {
        size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed == 0) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseBool(const std::string& text, bool& out) {
    if (text == "1" || text == "true"  || text == "yes") { out = true;  return true; }
    if (text == "0" || text == "false" || text == "no")  { out = false; return true; }
    return false;
}

void Apply(Settings& s, const std::string& key, const std::string& value) {
    if (key == "target.exe")            { s.targetExe = value; return; }
    if (key == "target.title_fallback") { s.titleFallback = value; return; }

    if (key == "detection.require_foreground") {
        ParseBool(value, s.requireForeground);
        return;
    }
    if (key == "detection.suppress_when_cursor_visible") {
        ParseBool(value, s.suppressWhenCursorVisible);
        return;
    }

    for (int i = 0; i < kSlotCountCfg; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "rate.slot%d", i + 1);
        if (key == name) {
            ParseFloat(value, s.ratePerSlot[static_cast<size_t>(i)]);
            return;
        }
    }

    if (key == "step.fine")     { ParseFloat(value, s.stepFine);    return; }
    if (key == "step.coarse")   { ParseFloat(value, s.stepCoarse);  return; }
    if (key == "preset.f7")     { ParseFloat(value, s.presetF7);    return; }
    if (key == "preset.f8")     { ParseFloat(value, s.presetF8);    return; }
    if (key == "hud.font_size") { ParseFloat(value, s.hudFontSize); return; }
    // Chave desconhecida: ignorada de proposito.
}

}  // namespace

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                           static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        out.data(), needed);
    return out;
}

std::wstring Path() {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return kFileName;

    std::wstring path(exePath, len);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return kFileName;

    path.resize(slash + 1);
    path += kFileName;
    return path;
}

Settings Load() {
    Settings settings;

    std::ifstream file(Path());
    if (!file) return settings;  // sem arquivo: padroes

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        Apply(settings, Trim(std::string_view(trimmed).substr(0, eq)),
                        Trim(std::string_view(trimmed).substr(eq + 1)));
    }
    return settings;
}

bool Save(const Settings& s) {
    std::ofstream file(Path(), std::ios::trunc);
    if (!file) return false;

    file << "# vertical-aim-controller\n"
            "# Gerado automaticamente. Editavel a mao; o programa reescreve o\n"
            "# arquivo ao sair, entao comentarios adicionados aqui se perdem.\n\n";

    file << "target.exe = "            << s.targetExe     << "\n";
    file << "target.title_fallback = " << s.titleFallback << "\n\n";

    file << "detection.require_foreground = "
         << (s.requireForeground ? 1 : 0) << "\n";
    file << "detection.suppress_when_cursor_visible = "
         << (s.suppressWhenCursorVisible ? 1 : 0) << "\n\n";

    file << "# Compensacao em pixels por segundo, por slot de arma.\n";
    for (int i = 0; i < kSlotCountCfg; ++i) {
        file << "rate.slot" << (i + 1) << " = "
             << s.ratePerSlot[static_cast<size_t>(i)] << "\n";
    }
    file << "\n";

    file << "step.fine = "   << s.stepFine   << "\n";
    file << "step.coarse = " << s.stepCoarse << "\n";
    file << "preset.f7 = "   << s.presetF7   << "\n";
    file << "preset.f8 = "   << s.presetF8   << "\n\n";

    file << "hud.font_size = " << s.hudFontSize << "\n";

    return file.good();
}

}  // namespace config
