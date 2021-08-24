#include "daisy_patch.h"
#include "daisysp.h"
#include "envelope.h"
#include <string>
#define NUM_VOICES 32
#define MAX_DELAY ((size_t)(10.0f * 48000.0f))
using namespace daisy;
using namespace daisysp;

PolyPluck<NUM_VOICES> synth;

float sampleRate;
float gain;
float threshold;
float buf[9600];

DaisyPatch hw;

bool gate;
bool audioGate;

// Persistent filtered Value for smooth delay time changes.
float smooth_time;
float nn;

TranceGate::Envelope env;
Comb              flt;
PitchShifter      pitchShift;
void UpdateControls()
{
    hw.ProcessAnalogControls();
	//hw.ProcessDigitalControls();
	env.SetRise(hw.GetKnobValue(DaisyPatch::CTRL_5)); // value expected to be from 0.f to 1.f
    env.SetFall(hw.GetKnobValue(DaisyPatch::CTRL_6)); // idk what this is gonna output 0-4095?
	flt.SetRevTime(hw.GetKnobValue(DaisyPatch::CTRL_3));
	flt.SetFreq(hw.GetKnobValue(DaisyPatch::CTRL_4) * 400 + 40);
	pitchShift.SetDelSize(hw.GetKnobValue(DaisyPatch::CTRL_1) * 500 + 25); //hopefully milliseconds!!
	pitchShift.SetTransposition(hw.GetKnobValue(DaisyPatch::CTRL_2) * 12);//hopefully semitones

	// gain = hw.GetKnobValue(DaisyPatch::CTRL_1);
	// threshold = hw.GetKnobValue(DaisyPatch::CTRL_4);
	
	gate = hw.gate_input[DaisyPatch::GATE_IN_1].Trig();

	if(gate)
    {
        env.Trigger();
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	UpdateControls();
	float output;
	float sig;
    float trig, decay;       // Pluck Vars

    // Set MIDI Note for new Pluck notes.
    //nn = 24.0f + hw.GetKnobValue(DaisyPatch::CTRL_1) * 60.0f;
    nn = static_cast<int32_t>(nn); // Quantize to semitones

    // Read knobs for decay;
    decay = 0.5f + (hw.GetKnobValue(DaisyPatch::CTRL_2) * 0.5f);
    synth.SetDecay(decay);

	for (size_t i = 0; i < size; i++) // size is number of channels two in this case
	{
        // Synthesize Plucks
		 if (in[0][i] > 0.01)
		 {
		    trig = in[0][i];
		 }
		 // do something here when num voices is more than 1
		 
         sig = synth.Process(trig, nn);
		
		 output = env.Process() * in[0][i];
		 output = pitchShift.Process(output);
		 output = 0.5 * flt.Process(output);
		 out[0][i] = sig;
		 out[1][i] = env.Process() * in[1][i]; 
	}
	
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                //osc.SetFreq(mtof(p.note));
				nn = p.note;
                //osc.SetAmp((p.velocity / 127.0f));
            }

            // if(m.data[1] == 0)
            // {
            //     p = m.AsNoteOn();
            //     osc.SetFreq(mtof(p.note));
            //     osc.SetAmp((0.0f));
            // }
        }
        break;

        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
                    //filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
                    //filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: break;
            }
            break;
        }
        default: break;
    }
}

int main(void)
{
	hw.Init();
	sampleRate = hw.AudioSampleRate();

	env.Init(sampleRate);
	    for(int i = 0; i < 9600; i++)
    {
        buf[i] = 0.0f;
    }

    // initialize Comb object
    flt.Init(sampleRate, buf, 9600);
    flt.SetFreq(500);
	pitchShift.Init(sampleRate);
	synth.Init(sampleRate);
	hw.midi.StartReceive();
	hw.StartAdc();
	hw.StartAudio(AudioCallback);
	for(;;)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }
    }
}


