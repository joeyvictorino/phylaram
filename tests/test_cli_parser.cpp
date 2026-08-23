#include <cassert>
#include <string>
#include <vector>
#include <iostream>

struct CliOptions {
    std::wstring output;
    bool quiet = false;
    bool hashEnabled = true;
    bool dryRun = false;
    bool jsonOutput = false;
    uint32_t rateLimitMBps = 0;
    bool showHelp = false;
    bool valid = false;
};

CliOptions ParseArgs(const std::vector<std::wstring>& args) {
    CliOptions opt;
    if (args.empty()) {
        opt.valid = false;
        return opt;
    }

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == L"--quiet") {
            opt.quiet = true;
        } else if (arg == L"--no-hash") {
            opt.hashEnabled = false;
        } else if (arg == L"--dry-run") {
            opt.dryRun = true;
        } else if (arg == L"--json") {
            opt.jsonOutput = true;
        } else if (arg == L"--rate-limit" || arg == L"--throttle") {
            if (i + 1 < args.size()) {
                opt.rateLimitMBps = static_cast<uint32_t>(std::stoi(args[++i]));
            } else {
                opt.valid = false;
                return opt;
            }
        } else if (arg == L"--help" || arg == L"-h") {
            opt.showHelp = true;
            opt.valid = true;
            return opt;
        } else if (!arg.empty() && arg[0] == L'-' && arg != L"-") {
            opt.valid = false;
            return opt;
        } else if (opt.output.empty()) {
            opt.output = arg;
        } else {
            // Extra positional arg
            opt.valid = false;
            return opt;
        }
    }

    opt.valid = opt.showHelp || opt.dryRun || !opt.output.empty();
    return opt;
}

int main() {
    // 1. Standard valid invocation
    {
        auto opt = ParseArgs({L"memory.raw"});
        assert(opt.valid);
        assert(opt.output == L"memory.raw");
        assert(!opt.quiet);
        assert(opt.hashEnabled);
        assert(opt.rateLimitMBps == 0);
        assert(!opt.showHelp);
        assert(!opt.dryRun);
        assert(!opt.jsonOutput);
    }

    // 2. Advanced flags: --quiet, --no-hash, --rate-limit
    {
        auto opt = ParseArgs({L"C:\\Evidence\\image.raw", L"--quiet", L"--no-hash", L"--rate-limit", L"250"});
        assert(opt.valid);
        assert(opt.output == L"C:\\Evidence\\image.raw");
        assert(opt.quiet);
        assert(!opt.hashEnabled);
        assert(opt.rateLimitMBps == 250);
    }

    // 3. Stdout streaming "-"
    {
        auto opt = ParseArgs({L"-"});
        assert(opt.valid);
        assert(opt.output == L"-");
    }

    // 4. Help flags
    {
        auto opt1 = ParseArgs({L"--help"});
        assert(opt1.valid && opt1.showHelp);
        auto opt2 = ParseArgs({L"-h"});
        assert(opt2.valid && opt2.showHelp);
    }

    // 5. Dry-run and JSON telemetry flags
    {
        auto opt1 = ParseArgs({L"--dry-run"});
        assert(opt1.valid);
        assert(opt1.dryRun);
        assert(!opt1.jsonOutput);

        auto opt2 = ParseArgs({L"--dry-run", L"--json"});
        assert(opt2.valid);
        assert(opt2.dryRun);
        assert(opt2.jsonOutput);

        auto opt3 = ParseArgs({L"image.raw", L"--json"});
        assert(opt3.valid);
        assert(!opt3.dryRun);
        assert(opt3.jsonOutput);
    }

    // 6. Invalid flag rejection
    {
        auto opt = ParseArgs({L"memory.raw", L"--compress"});
        assert(!opt.valid);
    }

    // 7. Missing rate-limit argument rejection
    {
        auto opt = ParseArgs({L"memory.raw", L"--rate-limit"});
        assert(!opt.valid);
    }

    // 8. Multiple positional files rejection
    {
        auto opt = ParseArgs({L"memory.raw", L"extra.raw"});
        assert(!opt.valid);
    }

    std::cout << "[PASS] CLI parser validation tests passed successfully.\n";
    return 0;
}
