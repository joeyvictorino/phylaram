#include <cassert>
#include <cstdint>
#include <cwctype>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

enum class Command {
    Capture,
    DryRun,
    Gui,
    Help,
};

enum class OutputFormat {
    Raw,
    Zdmp,
    E01,
};

struct EvidenceMetadata {
    std::wstring caseNumber;
    std::wstring evidenceNumber;
    std::wstring examiner;
    std::wstring description;
    std::wstring notes;
};

struct CliOptions {
    Command command = Command::Capture;
    std::wstring output;
    bool quiet = false;
    bool json = false;
    uint32_t rateLimitMBps = 0;
    OutputFormat format = OutputFormat::Raw;
    EvidenceMetadata metadata;
    bool valid = false;
};

bool ParseUint32(const std::wstring& text, uint32_t& value)
{
    if (text.empty()) {
        return false;
    }

    uint64_t parsed = 0;
    for (const wchar_t character : text) {
        if (std::iswdigit(character) == 0) {
            return false;
        }
        const uint32_t digit = static_cast<uint32_t>(character - L'0');
        if (parsed > (std::numeric_limits<uint32_t>::max() - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

CliOptions ParseArgs(const std::vector<std::wstring>& args)
{
    CliOptions options;
    if (args.empty()) {
        options.command = Command::Gui;
        options.valid = true;
        return options;
    }

    bool sawGui = false;
    bool sawHelp = false;
    bool sawDryRun = false;

    for (size_t index = 0; index < args.size(); ++index) {
        const std::wstring& argument = args[index];
        if (argument == L"--gui") {
            sawGui = true;
        } else if (argument == L"--help" || argument == L"-h") {
            sawHelp = true;
        } else if (argument == L"--dry-run") {
            sawDryRun = true;
        } else if (argument == L"--quiet") {
            options.quiet = true;
        } else if (argument == L"--json") {
            options.json = true;
        } else if (argument == L"--format") {
            if (index + 1 >= args.size()) {
                return options;
            }
            std::wstring fmt = args[++index];
            for (auto& c : fmt) {
                c = static_cast<wchar_t>(towlower(c));
            }
            if (fmt == L"raw") {
                options.format = OutputFormat::Raw;
            } else if (fmt == L"zdmp" || fmt == L"dmp") {
                options.format = OutputFormat::Zdmp;
            } else if (fmt == L"e01") {
                options.format = OutputFormat::E01;
            } else {
                return options;
            }
        } else if (argument == L"--case-number") {
            if (index + 1 >= args.size()) return options;
            options.metadata.caseNumber = args[++index];
        } else if (argument == L"--evidence-number") {
            if (index + 1 >= args.size()) return options;
            options.metadata.evidenceNumber = args[++index];
        } else if (argument == L"--examiner") {
            if (index + 1 >= args.size()) return options;
            options.metadata.examiner = args[++index];
        } else if (argument == L"--description") {
            if (index + 1 >= args.size()) return options;
            options.metadata.description = args[++index];
        } else if (argument == L"--notes") {
            if (index + 1 >= args.size()) return options;
            options.metadata.notes = args[++index];
        } else if (argument == L"--rate-limit") {
            if (index + 1 >= args.size() ||
                !ParseUint32(args[++index], options.rateLimitMBps)) {
                return options;
            }
        } else if (!argument.empty() && argument.front() == L'-') {
            return options;
        } else if (options.output.empty()) {
            options.output = argument;
        } else {
            return options;
        }
    }

    const unsigned modeCount = static_cast<unsigned>(sawGui) +
                               static_cast<unsigned>(sawHelp) +
                               static_cast<unsigned>(sawDryRun);
    if (modeCount > 1) {
        return options;
    }

    if (sawHelp) {
        options.command = Command::Help;
        options.valid = options.output.empty() && !options.json &&
                        options.rateLimitMBps == 0 && !options.quiet;
        return options;
    }
    if (sawGui) {
        options.command = Command::Gui;
        options.valid = options.output.empty() && !options.json &&
                        options.rateLimitMBps == 0 && !options.quiet;
        return options;
    }
    if (sawDryRun) {
        options.command = Command::DryRun;
        options.valid = options.output.empty() && options.rateLimitMBps == 0;
        return options;
    }

    options.command = Command::Capture;
    options.valid = !options.output.empty() &&
                    options.output != L"-" &&
                    !options.json;
    return options;
}

int main()
{
    {
        const CliOptions options = ParseArgs({L"memory.raw"});
        assert(options.valid);
        assert(options.command == Command::Capture);
        assert(options.output == L"memory.raw");
        assert(options.rateLimitMBps == 0);
    }

    {
        const CliOptions options = ParseArgs(
            {L"C:\\Evidence\\image.raw", L"--quiet", L"--rate-limit", L"250"});
        assert(options.valid);
        assert(options.quiet);
        assert(options.rateLimitMBps == 250);
    }

    {
        const CliOptions oneMiB = ParseArgs(
            {L"memory.raw", L"--rate-limit", L"1"});
        assert(oneMiB.valid);
        assert(oneMiB.rateLimitMBps == 1);
    }

    {
        const CliOptions negative = ParseArgs(
            {L"memory.raw", L"--rate-limit", L"-1"});
        const CliOptions garbage = ParseArgs(
            {L"memory.raw", L"--rate-limit", L"25MB"});
        const CliOptions overflow = ParseArgs(
            {L"memory.raw", L"--rate-limit", L"4294967296"});
        assert(!negative.valid);
        assert(!garbage.valid);
        assert(!overflow.valid);
    }

    {
        assert(!ParseArgs({L"-"}).valid);
        assert(!ParseArgs({L"memory.raw", L"--no-hash"}).valid);
        assert(!ParseArgs({L"memory.raw", L"--throttle", L"100"}).valid);
    }

    {
        const CliOptions help = ParseArgs({L"--help"});
        const CliOptions shortHelp = ParseArgs({L"-h"});
        assert(help.valid && help.command == Command::Help);
        assert(shortHelp.valid && shortHelp.command == Command::Help);
    }

    {
        const CliOptions dryRun = ParseArgs({L"--dry-run"});
        const CliOptions dryRunJson = ParseArgs({L"--dry-run", L"--json"});
        const CliOptions captureJson = ParseArgs({L"memory.raw", L"--json"});
        assert(dryRun.valid && dryRun.command == Command::DryRun);
        assert(dryRunJson.valid && dryRunJson.json);
        assert(!captureJson.valid);
    }

    {
        assert(!ParseArgs({L"memory.raw", L"--compress"}).valid);
        assert(!ParseArgs({L"memory.raw", L"--rate-limit"}).valid);
        assert(!ParseArgs({L"memory.raw", L"extra.raw"}).valid);
        assert(!ParseArgs({L"--dry-run", L"memory.raw"}).valid);
        assert(!ParseArgs({L"--gui", L"--dry-run"}).valid);
    }

    {
        const CliOptions noArgs = ParseArgs({});
        const CliOptions gui = ParseArgs({L"--gui"});
        assert(noArgs.valid && noArgs.command == Command::Gui);
        assert(gui.valid && gui.command == Command::Gui);
    }

    {
        const CliOptions zdmp = ParseArgs({L"dump.zdmp", L"--format", L"zdmp"});
        const CliOptions e01 = ParseArgs({L"evidence.e01", L"--format", L"e01", L"--case-number", L"CASE-99", L"--examiner", L"Special Agent Fox"});
        assert(zdmp.valid && zdmp.format == OutputFormat::Zdmp);
        assert(e01.valid && e01.format == OutputFormat::E01);
        assert(e01.metadata.caseNumber == L"CASE-99");
        assert(e01.metadata.examiner == L"Special Agent Fox");

        assert(!ParseArgs({L"mem.raw", L"--format", L"invalid_fmt"}).valid);
    }

    std::cout << "[PASS] Strict CLI contract tests passed.\n";
    return 0;
}
