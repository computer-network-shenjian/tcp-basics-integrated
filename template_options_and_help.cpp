
struct Options
{
    float seed;
    uint  windowWidth;
    uint  windowHeight;
    bool  manualSeed;
    bool  wireframe;
    bool  showHelp;
    bool  fullscreen;
    bool  planeBox;
};

const Options DefaultOptions {0.f, 1024u, 720u, false, false, false, false, false};

void printHelp()
{
    std::cout << "Fly -- A flight simulator" << std::endl
              << "usage: Fly [options...]" << std::endl
              << std::endl
              << "-h   | --help        Print this help text and exit" << std::endl
              << "-w X | wX            Set window width to X (default: "
                << DefaultOptions.windowWidth << ")" << std::endl
              << "-H Y | HY            Set window height to Y (default: "
                << DefaultOptions.windowHeight << ")" << std::endl
              << "-s Z | sZ            Set seed to Z (default: random seed)" << std::endl
              << "-f   | --fullscreen  Set fullscreen mode (default: " << std::boolalpha
                << DefaultOptions.fullscreen << ")" << std::endl
              << "--wireframe          Render in wireframe mode (default: " << std::boolalpha
                << DefaultOptions.wireframe << ")" << std::endl
              << "--plane-box          Draw a bounding box around the plane (default: " << std::boolalpha
                << DefaultOptions.planeBox << ")" << std::endl
              << std::endl;
}

Options processArguments(int argc, char** argv)
{
    using namespace fly;

    std::vector<std::string> arguments;
    Options opts = DefaultOptions;

    for (int i = 1; i < argc; ++i)
        arguments.push_back(argv[i]);

    for (auto i = arguments.begin(); i != arguments.end(); ++i)
    {
        auto&& arg = *i;
        if (arg == "-f" || arg == "--fullscreen")
        {
            opts.fullscreen = true;
            LOG(Info) << "Window set to fullscreen." << std::endl;
        }
        else if (arg == "--wireframe")
        {
            opts.wireframe = true;
            LOG(Info) << "Rendering in wireframe mode." << std::endl;
        }
        else if (arg == "--plane-box")
        {
            opts.planeBox = true;
            LOG(Info) << "Drawing bounding box around plane." << std::endl;
        }
        else if (arg.substr(0, 2) == "-w")
        {
            auto width = arg.substr(2);
            if (width.empty() && std::next(i) != arguments.end())
                width = *++i;
            try
            {
                opts.windowWidth = std::stoi(width);
                LOG(Info) << "Window width set to " << opts.windowWidth << std::endl;
            }
            catch(std::exception error)
            {
                LOG(Error) << "Invalid parameter for window width" << std::endl;
            }
        }
        else if (arg.substr(0, 2) == "-H")
        {
            auto height = arg.substr(2);
            if (height.empty() && std::next(i) != arguments.end())
                height = *++i;
            try
            {
                opts.windowHeight = std::stoi(height);
                LOG(Info) << "Window height set to " << opts.windowHeight << std::endl;
            }
            catch(std::exception error)
            {
                LOG(Error) << "Invalid parameter for window height" << std::endl;
            }
        }
        else if (arg.substr(0, 2) == "-s")
        {
            auto seed = arg.substr(2);
            if (seed.empty() && std::next(i) != arguments.end())
                seed = *++i;
            try
            {
                opts.seed = std::stol(seed);
                opts.manualSeed = true;
                LOG(Info) << "Seed set to " << opts.seed << std::endl;
            }
            catch(std::exception error)
            {
                LOG(Error) << "Invalid parameter for seed" << std::endl;
            }
        }
        else if (arg == "-h" || arg == "--help")
        {
            opts.showHelp = true;
            break;
        }
    }

    return opts;
}

// Option usage
Options opts = processArguments(argc, argv);
if (opts.showHelp)
{
    printHelp();
    return EXIT_SUCCESS;
}
