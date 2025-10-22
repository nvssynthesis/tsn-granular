namespace nvs::timbrespace {

template<typename Point_t>
NavigationStrategy<Point_t>::NavigationStrategy(NavigationType_e type)    :   _navType(std::move(type)) {}

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
    :   NavigationStrategy<Point_t>(NavigationType_e::RandomWalk)
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
:   NavigationStrategy<Point_t>(NavigationType_e::LFO)
{
    for (auto &f : filters) {
        f.setSampleRate(0.0f);
    }
}

template<typename Point_t>
void LFONavigator<Point_t>::setSampleRate(double sampleRate) {
    NavigationStrategy<Point_t>::setSampleRate(sampleRate);
    for (auto &f : filters) {
        f.setSampleRate(sampleRate);
    }
}

template<typename Point_t>
Point_t LFONavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space,
    Point_t previousPoint)
{
    setFrequency(*paramTree.getRawParameterValue("nav_lfo_rate"));
    const double filterCutoff = *paramTree.getRawParameterValue("nav_lfo_response");
    const double filterReso = *paramTree.getRawParameterValue("nav_lfo_overshoot");
    for (auto &f : filters) {
        f.setParams(filterCutoff, filterReso);
    }

    const double amplitude = *paramTree.getRawParameterValue("nav_lfo_amount");

    this->_centers[0] = *paramTree.getRawParameterValue("nav_tendency_x");
    this->_centers[1] = *paramTree.getRawParameterValue("nav_tendency_y");

    // compute phase and waveforms
    assert(this->_phaseIncrement >= 0.0);	// otherwise wrapping method won't work as expected
    this->_phase += this->_phaseIncrement;
    if (this->_phase > 2.0 * juce::MathConstants<double>::pi){
        this->_phase -= 2.0 * juce::MathConstants<double>::pi;
    }

    float shapeValue = *paramTree.getRawParameterValue("nav_lfo_shape");

    auto calculateShape = [shapeValue](const double phase){
        double a = shapeValue;

        auto i = [](double d){return int(d);};
        auto frac = [i](double d){return d - i(d);};
        auto tri = [](double d){ return 2.0 * (d < 0.5 ? d : 1 - d); };

        double p = 4.0 * tri(shapeValue);
        double q = 2.0 * a * a;

        return (1.0 - frac(q)) * std::cos((i(q) + 1.0) * phase) + frac(q) * std::cos((i(q) + 2.0) * phase);
    };
    Point_t p {};
    for (int i = 0; i < this->Dimensions; ++i) {
        const double interDimensionalPhaseIncrement = 2.5 * (static_cast<double>(i) / this->Dimensions) * -M_PI;
        const double ø = this->_phase + interDimensionalPhaseIncrement;
        p[i] = this->filters[i](amplitude * calculateShape(ø) + this->_centers[i]);
    }

    return p;
}
template<typename Point_t>
void LFONavigator<Point_t>::setFrequency(const double frequency) {
    jassert (this->_sampleRate != 0.0);
    _phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequency / this->_sampleRate;
}


template<typename Point_t>
LorenzNavigator<Point_t>::LorenzNavigator()
    :   NavigationStrategy<Point_t>(NavigationType_e::Lorenz)
{}
enum class IntegrationMethod {
    Euler = 0,
    RK4 = 1
};

static constexpr IntegrationMethod integrationMethod {IntegrationMethod::RK4};

template<typename Point_t>
Point_t LorenzNavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) {
    {
        const double a = *paramTree.getRawParameterValue("nav_lorenz_a");
        const double b = *paramTree.getRawParameterValue("nav_lorenz_b");
        const double c = *paramTree.getRawParameterValue("nav_lorenz_c");
        const double d_t = *paramTree.getRawParameterValue("nav_lorenz_d_t");
        auto lorenz = [&](double x, double y, double z) -> std::tuple<double, double, double> {
            return {
                a * (y - x),
                x * (b - z) - y,
                (x * y) - (c * z)
            };
        };

        if constexpr (integrationMethod == IntegrationMethod::Euler) {
            auto [x_inc, y_inc, z_inc] = lorenz(_x, _y, _z);

            _x += x_inc * d_t;
            _y += y_inc * d_t;
            _z += z_inc * d_t;
        }
        else if constexpr (integrationMethod == IntegrationMethod::RK4) {
            const double x = _x;
            const double y = _y;
            const double z = _z;

            // RK4 stages
            auto [k1_x, k1_y, k1_z] = lorenz(x, y, z);

            auto [k2_x, k2_y, k2_z] = lorenz(
                x + 0.5 * d_t * k1_x,
                y + 0.5 * d_t * k1_y,
                z + 0.5 * d_t * k1_z
            );

            auto [k3_x, k3_y, k3_z] = lorenz(
                x + 0.5 * d_t * k2_x,
                y + 0.5 * d_t * k2_y,
                z + 0.5 * d_t * k2_z
            );

            auto [k4_x, k4_y, k4_z] = lorenz(
                x + d_t * k3_x,
                y + d_t * k3_y,
                z + d_t * k3_z
            );

            // Apply RK4 weights
            _x += (d_t / 6.0) * (k1_x + 2.0*k2_x + 2.0*k3_x + k4_x);
            _y += (d_t / 6.0) * (k1_y + 2.0*k2_y + 2.0*k3_y + k4_y);
            _z += (d_t / 6.0) * (k1_z + 2.0*k2_z + 2.0*k3_z + k4_z);
        }
    }

    Point_t p = Point_t::Zero();

    if constexpr (1 <= NavigationStrategy<Point_t>::Dimensions) {
        p[0] = _x * 0.05;
    }
    if constexpr (2 <= NavigationStrategy<Point_t>::Dimensions) {
        p[1] = _y * 0.05;
    }
    if constexpr (3 <= NavigationStrategy<Point_t>::Dimensions) {
        p[2] = (_z - 28.0) * 0.05;  // hack of approximately removing DC offset, based on guestimate observation
    }

    return p;
}


template<typename Point_t>
HyperchaosNavigator<Point_t>::HyperchaosNavigator()
    :   NavigationStrategy<Point_t>(NavigationType_e::Hyperchaos)
{}

static const double sgn(const double d) {
    return std::tanh(200.0 * d);
};

template<typename Point_t>
Point_t HyperchaosNavigator<Point_t>::navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) {
    {
        const double a = *paramTree.getRawParameterValue("nav_hyperchaos_a");
        const double b = *paramTree.getRawParameterValue("nav_hyperchaos_b");
        const double d_t = *paramTree.getRawParameterValue("nav_hyperchaos_d_t");

        auto hyperchaos = [&](double x, double y, double z, double u) -> std::tuple<double, double, double, double> {
            return {
                y - x,
                -1.0 * z * sgn(x) + u,
                x * sgn(y) - a,
                -1.0 * b * y
            };
        };

        if constexpr (integrationMethod == IntegrationMethod::Euler) {
            auto [x_inc, y_inc, z_inc, u_inc] = hyperchaos(_x, _y, _z, _u);

            _x += x_inc * d_t;
            _y += y_inc * d_t;
            _z += z_inc * d_t;
            _u += u_inc * d_t;
        }
        else if constexpr (integrationMethod == IntegrationMethod::RK4) {
            const double x = _x;
            const double y = _y;
            const double z = _z;
            const double u = _u;

            // RK4 stages
            auto [k1_x, k1_y, k1_z, k1_u] = hyperchaos(x, y, z, u);

            auto [k2_x, k2_y, k2_z, k2_u] = hyperchaos(
                x + 0.5 * d_t * k1_x,
                y + 0.5 * d_t * k1_y,
                z + 0.5 * d_t * k1_z,
                u + 0.5 * d_t * k1_u
            );

            auto [k3_x, k3_y, k3_z, k3_u] = hyperchaos(
                x + 0.5 * d_t * k2_x,
                y + 0.5 * d_t * k2_y,
                z + 0.5 * d_t * k2_z,
                u + 0.5 * d_t * k2_u
            );

            auto [k4_x, k4_y, k4_z, k4_u] = hyperchaos(
                x + d_t * k3_x,
                y + d_t * k3_y,
                z + d_t * k3_z,
                u + d_t * k3_u
            );

            // Combine with RK4 weights
            _x += (d_t / 6.0) * (k1_x + 2.0*k2_x + 2.0*k3_x + k4_x);
            _y += (d_t / 6.0) * (k1_y + 2.0*k2_y + 2.0*k3_y + k4_y);
            _z += (d_t / 6.0) * (k1_z + 2.0*k2_z + 2.0*k3_z + k4_z);
            _u += (d_t / 6.0) * (k1_u + 2.0*k2_u + 2.0*k3_u + k4_u);
        }
    }

    Point_t p = Point_t::Zero();

    if constexpr (1 <= NavigationStrategy<Point_t>::Dimensions) {
        p[0] = _x;
    }
    if constexpr (2 <= NavigationStrategy<Point_t>::Dimensions) {
        p[1] = _y;
    }
    if constexpr (3 <= NavigationStrategy<Point_t>::Dimensions) {
        p[2] = _z;
    }
    if constexpr (4 <= NavigationStrategy<Point_t>::Dimensions) {
        p[3] = _u;
    }

    return p * 0.2f;
}

template<typename Point_t>
Navigator<Point_t>::Navigator(const AudioProcessorValueTreeState &paramTree)
:   _apvts(paramTree) {}
template<typename Point_t>
void Navigator<Point_t>::setNavigationStrategy(const NavigationType_e navType) {
    switch (navType) {
        case NavigationType_e::LFO:
            _navigationStrategy = std::make_unique<LFONavigator<Point_t>>();
            break;
        case NavigationType_e::RandomWalk:
            _navigationStrategy = std::make_unique<RandomWalkNavigator<Point_t>>();
            break;
        case NavigationType_e::Lorenz:
            _navigationStrategy = std::make_unique<LorenzNavigator<Point_t>>();
            break;
        case NavigationType_e::Hyperchaos:
            _navigationStrategy = std::make_unique<HyperchaosNavigator<Point_t>>();
    }
    updateStrategyRate();
}
template<typename Point_t>
void Navigator<Point_t>::setSampleRate(double sampleRate) {
    _sampleRate = sampleRate;
    updateNavPeriodSamples();
    if (_navigationStrategy != nullptr) {
        _navigationStrategy->setSampleRate(sampleRate);
    }
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
    if (_navigationStrategy == nullptr) {
        return Point_t::Zero();
    }

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
