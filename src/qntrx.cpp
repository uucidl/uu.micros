#include <micros/api.h>
#include <micros/gl3.h>

#include <vector>
#include <cmath>

const double TAU = 6.28318530717958647692528676655900576839433879875021;

/**
 * A set of values going from 0 to 1 at various speeds, representing
 * various cycles in the passage of time.
 *
 * They can be used to scan wavetables or be fed to functions such as
 * sin/cos etc.. to produce oscillators or envelopes.
 */
class Phasers
{
public:
        size_t create(double frequency, double offset = 0.0)
        {
                size_t const id = data.size();
                data.emplace_back(offset, frequency / 48000.0);
                return id;
        }

        size_t create_follower(size_t main, double ratio = 1.0, double offset = 0.0)
        {
                size_t const id = create(0.0, offset);
                followers.emplace_back(id, main, ratio);
                return id;
        }

        void change(size_t phaser, double frequency)
        {
                data[phaser].increment = frequency / 48000.0;
        }

        void offset(size_t phaser, double offset)
        {
                data[phaser].phase = offset;
        }

        double get(int phaser) const
        {
                return data[phaser].phase;
        }

        double get_radians(int phaser) const
        {
                return data[phaser].phase * TAU;
        }

        void advance()
        {
                for (auto& follower : followers) {
                        data[follower.id].increment = data[follower.main].increment * follower.ratio;
                }

                for (auto& phaser : data) {
                        phaser.phase =
                                fmod(phaser.phase + phaser.increment, 1.0);
                }
        }

private:
        struct state {
                double phase;
                double increment;
                state(double phase, double increment) :
                        phase(phase),
                        increment(increment) {}
        };
        std::vector<state> data;

        struct follower_state {
                size_t id;
                size_t main;
                double ratio;

                follower_state(size_t id, size_t main, double ratio) :
                        id(id),
                        main(main),
                        ratio(ratio) {}
        };

        std::vector<follower_state> followers;
};

static double sinexpenv(double phase, double attack_speed, double decay_speed)
{
        //double const attack = fmax(0.0, 1.0 + log10(fmin(1.0, attack_speed * phase)));
        double const attack_dur = 1.0 / attack_speed;
        double const attack = fmax(0.0, sin(fmin (attack_dur,
                                            phase) * TAU * attack_speed / 4.0));
        double const decay = fmin(1.0,
                                  exp(-decay_speed * phase + decay_speed/attack_speed));
        return attack * decay * fmin(1.0, 1000.0 * (1.0 - phase));
}

static void phaser_tweak(Phasers& phasers,
                         size_t const tweaked,
                         size_t const original,
                         size_t const shifter)
{
        double const original_phase = phasers.get(original);
        if (phasers.get(shifter) == 0.0) {
                phasers.offset(tweaked, original_phase);
        }
        phasers.change(tweaked, original);
}

static double phaser_fbmodulate(Phasers& phasers,
                                size_t const main,
                                size_t const modulator,
                                double const frequency,
                                double const cm_frequency_ratio,
                                double const modulation_index,
                                double const feedback)
{
        double const osc_radians = phasers.get_radians(main);
        double const mod_radians = phasers.get_radians(modulator);
        double const modulation = modulation_index * sin(mod_radians);
        double const main_freq = frequency + modulation;

        phasers.change(modulator,
                       feedback * sin(osc_radians) + frequency / cm_frequency_ratio);
        phasers.change(main, main_freq);

        return main_freq;
}

static double phaser_n(double phase, double q)
{
        return fmod(q * phase, 1.0);
}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        static Phasers phasers;

        static struct SharedPhasers {
                size_t shifter = phasers.create(48000.0 / 64.0);
                size_t measure = phasers.create(0.50);
                size_t sometime = phasers.create_follower(measure, 1.0 / 16.0 / 8.0);
        } shared_phasers;

        static struct KickPhasers {
                size_t a = phasers.create_follower(shared_phasers.measure, 4.0);
                size_t aa = phasers.create_follower(a);
                size_t osc = phasers.create(50.0);
        } kick_phasers;

        struct Kick {
                double freq_env_base = 50.0;
                double freq_env_amp = 1500.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 80.0;
                double amplitude_env_accel = 2000.0;
                double amplitude_env_decay = 4.0;
        } kick;

        static struct BounceKickPhasers {
                size_t osc = phasers.create_follower(kick_phasers.osc);
        } bounce_kick_phasers;

        struct BounceKick {
                double freq_env_base = 50.0;
                double amplitude_env_accel = 80.0;
                double amplitude_env_decay = 20.0;
        } bounce_kick;

        static struct SnarePhasers {
                size_t a = phasers.create_follower(kick_phasers.a, 1.0/2.0, 0.50);
                size_t osc = phasers.create(180.0);
                size_t mod_osc = phasers.create(90.0);
        } snare_phasers;

        struct Snare {
                double freq_env_base = 50.0 * 1.5;
                double freq_env_amp = 3000.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 90.0;
                double amplitude_env_accel = 1200.0;
                double amplitude_env_decay = 13.0;
                double modulator_freq_ratio = 1.0 / sqrt(1.5);
                double modulator_index = 800.0;
                double feedback = 1600.0;
        } snare;


        struct Hihat {
                double freq_env_base = 50.0 * 4.0;
                double freq_env_amp = 600.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 90.0;
                double amplitude_env_accel = 1200.0;
                double amplitude_env_decay = 13.0;
                double modulator_freq_ratio = 1.0 / sqrt(3.0);
                double modulator_index = 800.0;
                double feedback = 0.97;
        } hihat;

        static struct HihatPhasers {
                size_t a = phasers.create_follower(kick_phasers.a, 1.0/2.0, 0.50);
                size_t osc = phasers.create(180.0);
                size_t mod_osc = phasers.create(90.0);
        } hihat_phasers;

        static struct BassPhasers {
                size_t b = phasers.create(4.0);
                size_t ba = phasers.create_follower(b);
                size_t osc = phasers.create(110.0);
                size_t sweep = phasers.create(1.0/32.0);
                size_t modulator_osc = phasers.create(110.0);
        } bass_phasers;

        struct Bass {
                double modulator_freq_ratio = 0.5;
                double modulator_amp = 3.0;
                double freq_env_base = 110.0;
                double freq_env_amp = 1500.0;
                double freq_env_accel = 400.0;
                double freq_env_decay = 40.0;
                double amplitude_env_accel = 8.0;
                double amplitude_env_decay = 3.0;
        } bass;

        static struct MidPhasers {
                size_t m = phasers.create(1.0/16.0, 0.750);
                size_t root_osc = phasers.create(220.0);
                size_t modulator_osc = phasers.create(220.0);
                size_t detuned_osc = phasers.create_follower(root_osc, 1.0037);
                size_t major[2];
                size_t minor[2];

                MidPhasers()
                {
                        major[0] = phasers.create_follower(root_osc, 5.0 / 4.0);
                        minor[1] = phasers.create_follower(root_osc, 6.0 / 4.0);
                        minor[0] = phasers.create_follower(root_osc, 12.0 / 10.0);
                        minor[1] = phasers.create_follower(root_osc, 15.0 / 10.0);
                }
        } mid_phasers;

        struct Mid {
                double modulator_freq_ratio = 2.00;
                double modulator_amp = 15.0;
                double modulator_fb = 0.2570;
                double amplitude_env_accel = 5.0;
                double amplitude_env_decay = 6.0;
        } mid;

        bool kick_track = true;
        bool bounce_kick_track = true;
        bool snare_track = true;
        bool hihat_track = true;
        bool mid_track = true;

        double const kick_gain = 0.5;
        double const kick_bounce_gain = 0.5;
        double const snare_gain = 0.4;
        double const hihat_gain = 0.5;
        double const mid_gain = 0.25;
        double const master_gain = 0.5;

        double const bpm = 133.0;
        phasers.change(shared_phasers.measure, bpm / 120.0 * 0.50);

        for (int i = 0; i < sample_count; i++) {
                left[i] = 0.0;
                right[i] = 0.0;

                bool all_measures_but_last =
                        floor(fmod(1.0 +
                                   phasers.get(shared_phasers.sometime) * 16.0,
                                   16.0)) > 0.0;

                kick_track = all_measures_but_last;
                bounce_kick_track = kick_track;

                if (kick_track) {
                        auto const& note_phaser = kick_phasers.a;
                        auto const& expression_phaser = kick_phasers.aa;
                        phaser_tweak(phasers,
                                     expression_phaser,
                                     note_phaser,
                                     shared_phasers.shifter);

                        auto const& note_phase = phasers.get(note_phaser);
                        auto const& expression_phase = phasers.get(expression_phaser);
                        auto const& params = kick;
                        auto const& voice = kick_phasers;
                        auto const& gain = kick_gain;

                        double const amplitude =
                                sinexpenv(expression_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);

                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phasers.change(voice.osc, freq);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians);
                }

                if (bounce_kick_track) {
                        double const measure = phasers.get(shared_phasers.measure);
                        double const beat = 16.0 * measure;
                        double const first = (beat >= 3.0
                                              && beat < 4.0) ? phaser_n(measure, 16.0) : 0.0;
                        double const second = (beat >= 6.0
                                               && beat < 8.0) ? phaser_n(measure, 8.0) : 0.0;

                        auto const& note_phase = first + second;
                        auto const& params = bounce_kick;
                        auto const& voice = bounce_kick_phasers;
                        auto const& gain = kick_bounce_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians);
                }

                if (snare_track) {
                        auto const& note_phaser = snare_phasers.a;

                        auto const& note_phase = phasers.get(note_phaser);
                        auto const& params = snare;
                        auto const& voice = snare_phasers;
                        auto const& gain = snare_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);
                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.osc,
                                          voice.mod_osc,
                                          freq,
                                          params.modulator_freq_ratio,
                                          params.modulator_index,
                                          params.feedback);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                }

                if (hihat_track) {
                        double const measure = phasers.get(shared_phasers.measure);
                        double const beat    = floor(measure * 16.0);
                        double const mask    = fmod(beat + 2.0, 4.0) > 0.0 ? 0.0 : 1.0;
                        double const open_mask =
                                floor(fmod(beat + 7.0, 16.0) / 2.0) > 0.0 ? 0.0 : 1.0;
                        double const phase   =
                                fmod(mask * phaser_n(measure, 16.0) +
                                     open_mask * phaser_n(measure, 8.0), 1.0);

                        auto const& note_phase = phase;
                        auto const& params = hihat;
                        auto const& voice = hihat_phasers;
                        auto const& gain = hihat_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);
                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.osc,
                                          voice.mod_osc,
                                          freq,
                                          params.modulator_freq_ratio,
                                          params.modulator_index,
                                          params.feedback);

                        double const osc_radians = phasers.get_radians(voice.osc);

                        left[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                }

                if (mid_track) {
                        auto const& note_phaser = mid_phasers.m;
                        auto const& params = mid;
                        auto const& voice = mid_phasers;
                        auto const& gain = mid_gain;

                        double const note_phase = phasers.get(note_phaser);
                        double const section = fmod(floor(shared_phasers.sometime * 16.0 * 4.0), 2.0);
                        double const minor_phase = (section == 0.0) ? note_phase : 0.0;
                        double const major_phase = (section == 1.0) ? note_phase : 0.0;
                        double amplitude = sinexpenv(note_phase,
                                                     params.amplitude_env_accel,
                                                     params.amplitude_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.root_osc,
                                          voice.modulator_osc,
                                          50.0 * 5,
                                          params.modulator_freq_ratio,
                                          amplitude * params.modulator_amp,
                                          params.modulator_fb);

                        for (auto const& phaser : voice.major) {
                                double amplitude = sinexpenv(major_phase,
                                                             params.amplitude_env_accel,
                                                             params.amplitude_env_decay);

                                double const osc = amplitude * cos(phasers.get_radians(phaser));

                                left[i] += gain * osc;
                                right[i] += gain * osc;
                        }

                        for (auto const& phaser : voice.minor) {
                                double amplitude = sinexpenv(minor_phase,
                                                             params.amplitude_env_accel,
                                                             params.amplitude_env_decay);

                                double const osc = amplitude * cos(phasers.get_radians(phaser));

                                left[i] += gain * osc;
                                right[i] += gain * osc;
                        }

                        double const osc = amplitude * cos(phasers.get_radians(voice.root_osc));

                        double const detuned_osc = amplitude * cos(phasers.get_radians(
                                                           voice.detuned_osc));

                        left[i] += gain * (0.55 * osc + 0.45 * detuned_osc);
                        right[i] += gain * (0.45 * osc + 0.55 * detuned_osc);
                }

                left[i] *= master_gain;
                right[i] *= master_gain;

                phasers.advance();
        }
}

extern void render_next_gl3(uint64_t time_micros, struct Display)
{
        glClearColor (0.2f, 0.2f, 0.3f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;

        runtime_init();

        return 0;
}
