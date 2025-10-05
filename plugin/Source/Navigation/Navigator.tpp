namespace nvs::timbrespace {

template<typename Point_t>
NavigationStrategy<Point_t>::NavigationStrategy(String name)    :   _name(std::move(name)) {}

template<typename Point_t>
void NavigationStrategy<Point_t>::setSampleRate(double sampleRate) {
    _sampleRate = sampleRate;
}
namespace {
using namespace std::string_view_literals;
static constexpr std::array navTendencyLookup {
    "nav_tendency_x"sv,
    "nav_tendency_y"sv,
    "nav_tendency_z"sv,
    "nav_tendency_u"sv,
    "nav_tendency_v"sv,
    "nav_tendency_w"sv
};
}

template<typename Point_t>
RandomWalkNavigator<Point_t>::RandomWalkNavigator()
    :   NavigationStrategy<Point_t>("RandomWalkNavigator")
{}

inline float randomStep1D(float previous, float tendency, float stepSize,
    std::mt19937 &rng, std::uniform_real_distribution<float> &uni)
{
    // 1) direction bias back toward 0
    float pUp = (tendency - previous) * 0.5 + 0.5;

    // 2) magnitude biased to extremes via sqrt transform
    float u = uni(rng);
    float magnitude = std::sqrt(u) * stepSize;

    // 3) direction
    bool up = (uni(rng) < pUp);
    float accumVal = (up ? magnitude : -magnitude);

    // 4) apply and clamp
    previous += accumVal;
    if (previous >  1.0) previous =  1.0;
    if (previous < -1.0) previous = -1.0;
    return previous;
}
template<typename Point_t>
Point_t RandomWalkNavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) {
    Point_t p{};
    for (int i = 0; i < this->Dimensions; ++i) {
        const auto s = juce::String(std::string(navTendencyLookup[i]));
        const float tendency = *paramTree.getRawParameterValue(s);
        const float prev = previousPoint[i];
        const float stepSize = *paramTree.getRawParameterValue("nav_rwalk_step_size");
        p[i] = randomStep1D(prev, tendency, stepSize,
            _rng, _uni);
    }
    // Eigen::Vector3<float> p1;
    jassert(p.norm() < 100.f);
    return p;
}

template<typename Point_t>
LFONavigator<Point_t>::LFONavigator()
:   NavigationStrategy<Point_t>("LFONavigator")
{
    for (auto &f : filters) {
        f.setSampleRate(0.0f);
    }
}

template<typename Point_t>
Point_t LFONavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) {

    return Point_t::Zero();
}

template<typename Point_t>
LorenzNavigator<Point_t>::LorenzNavigator()
    :   NavigationStrategy<Point_t>("LorenzNavigator")
{}

template<typename Point_t>
Point_t LorenzNavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) {
    return Point_t::Zero();
}


template<typename Point_t>
Navigator<Point_t>::Navigator(const AudioProcessorValueTreeState &paramTree)
:   _apvts(paramTree) {}
template<typename Point_t>
void Navigator<Point_t>::setNavigationStrategy(const NavigationType_e navType) {
    juce::String s;
    switch (navType) {
        case NavigationType_e::LFO:
            s = "LFO";
            _navigationStrategy = std::make_unique<LFONavigator<Point_t>>();
            break;
        case NavigationType_e::RandomWalk:
            s = "RandomWalk";
            _navigationStrategy = std::make_unique<RandomWalkNavigator<Point_t>>();
            break;
        case NavigationType_e::Lorenz:
            s = "Lorenz";
            _navigationStrategy = std::make_unique<LorenzNavigator<Point_t>>();
            break;
    }
    updateStrategyRate();
}
template<typename Point_t>
void Navigator<Point_t>::setSampleRate(double sampleRate) {
    _sampleRate = sampleRate;
    updateNavPeriodSamples();
    _navigationStrategy->setSampleRate(sampleRate);
}
template<typename Point_t>
void Navigator<Point_t>:: setNavigationPeriod(double navPeriodMs) {
    _navPeriodMs = navPeriodMs;
    updateNavPeriodSamples();
}
template<typename Point_t>
void Navigator<Point_t>::updateNavPeriodSamples() {
    _navPeriodSamps = (_navPeriodMs / 1000.0) * _sampleRate;
    updateStrategyRate();
}
template<typename Point_t>
void Navigator<Point_t>::updateStrategyRate() {
    if (_navigationStrategy && _navPeriodMs > 0) {
        double effectiveRate = 1000.0 / _navPeriodMs;  // Hz
        _navigationStrategy->setSampleRate(effectiveRate);
    }
}
template<typename Point_t>
Point_t Navigator<Point_t>::process(TimbreSpace const &space, int numSamplesElapsed) {
    jassert (_navPeriodSamps > 0);
    const int numStepsToTake = [this](double numSamplesElapsed) {
        _sampleCounter += numSamplesElapsed;
        int stepsToTake = 0;
        while (_sampleCounter >= _navPeriodSamps) {
            ++stepsToTake;
            _sampleCounter -= _navPeriodSamps;
        }
        return stepsToTake;
    }(numSamplesElapsed);

    if (_navigationStrategy == nullptr) { return {}; }
    for (int i = 0; i < numStepsToTake; ++i) {
       _previousPoint = _navigationStrategy->navigate(_apvts, space, _previousPoint);
    }
    jassert(_previousPoint.norm() < 100.f);
    return _previousPoint;
}

inline void staticNavTest() {
    class DummyProcessor : public juce::AudioProcessor {
    public:
        const String getName() const override {return "";}
        void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {}
        void releaseResources() override {}
        void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        AudioProcessorEditor *createEditor() override { return nullptr; }
        bool hasEditor() const override { return true; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int index) override {}
        const String getProgramName(int index) override { return ""; }
        void changeProgramName(int index, const String &newName) override {}
        void getStateInformation(MemoryBlock &destData) override {}
        void setStateInformation(const void *data, int sizeInBytes) override {}
    private:
    };
    DummyProcessor dummy;
    const juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "", {});
    static Navigator<Timbre3DPoint> nav(apvts);
    nav.setNavigationStrategy(NavigationType_e::LFO);
}
}
