#include "OfflineRenderer.h"

#include <string>
#include <vector>

int main (int argc, char** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve (argc > 1 ? static_cast<size_t> (argc - 1) : 0);
    for (int index = 1; index < argc; ++index)
        arguments.emplace_back (argv[index]);
    return ReferenceHarness::runOfflineRendererCommand (arguments);
}
