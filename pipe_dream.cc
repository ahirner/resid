#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#include "sid.h"
#include "siddefs.h"

#define OUTPUTBUFFERSIZE 8192
#define CPU_FREQ 985248 //1023444.642857142857143
#define SAMPLE_FREQUENCY 44100

#define INSTR_TO_CYCLE 3

    //using namespace reSID;

    int main(int argc, char **argv)
{

    //poor man's audio overflow protection
    short* m_buffer = new short[OUTPUTBUFFERSIZE];

    reSID::SID sid;

    sid.set_sampling_parameters((double) CPU_FREQ, reSID::SAMPLE_FAST, (double)SAMPLE_FREQUENCY);

    unsigned int cmd;
    unsigned int instr_buf = 0;

    int c_read = read(0, &cmd, 4);
    if (c_read == 4) instr_buf++;

    int buf_pos = 0;
    int samples_in_frame = 0;

    while (instr_buf)
    {
        unsigned int cmd_next;
        int c_read = read(0, &cmd_next, 4);
        if (c_read == 4) instr_buf++;

        //printf("%d", c_read);
        //fflush(stdout);
        {
            //How long this state will be sampled
            unsigned int instrs = (cmd_next & (0x1FFF << 16)) >> 16;
            //reSID::cycle_count cycles = instrs * INSTR_TO_CYCLE;

            //This quantizing cycle_counts, together with not filling up samples after frame rocks for glitch!!
            reSID::cycle_count cycles = 5000;

            // FRAME
            if (cmd & (1 << 31))
            {
                unsigned int f_nr = cmd & 0xFFFF;

                fprintf(stderr, "FRAME %d | next instr in %04x\n", f_nr, instrs);
                //fprintf(stderr, " -- cmd: %08x\n     cmd_next: %08x    \n", cmd, cmd_next);
                
                //flush the last every frame until all is filled up
                if (true)
                {
                    int samples_needed = SAMPLE_FREQUENCY * 50 - samples_in_frame;
                    samples_needed = cycles;
                    while (samples_needed)
                    {
                        int sampled = sid.clock(samples_needed, (short *)m_buffer + buf_pos, MAX(OUTPUTBUFFERSIZE - buf_pos, samples_needed));
                        buf_pos += sampled;
                        samples_in_frame += sampled;

                        write(1, (short *)m_buffer, buf_pos * 2);
                    }
                    //reset
                    int samples_in_frame = 0;
                }
            }

            // INSTRUCTION
            else
            {
                reSID::reg8 reg = (cmd & 0xFF00) >> 8;
                reSID::reg8 val = cmd & 0xFF;
                
                //Poke and mini sampling in between frames
                if (reg <= 24){
                    sid.write(reg, val);

                    reSID::cycle_count cycles_before = cycles;
                    int sampled = sid.clock(cycles, (short *)m_buffer + buf_pos, OUTPUTBUFFERSIZE - buf_pos);
                    buf_pos += sampled;
                    samples_in_frame += sampled;

                    fprintf(stderr, "%02x%02x: received %08x samples for %d cycles (now %d)\n", reg, val, sampled, cycles_before, cycles);

                    //sid.clock(cycles);
                }
                //Handle special instructions
                else {

                }
            }
            
            instr_buf--;
            cmd = cmd_next;
        }

        if (buf_pos >= OUTPUTBUFFERSIZE)
        {
            write(1, (short *)m_buffer, buf_pos*2);
            buf_pos = 0;
        }
    }

    delete[] m_buffer;
}

//reSID::cycle_count cycles = eventScheduler->getTime(m_accessClk, EVENT_CLOCK_PHI1);
//m_accessClk += cycles;
//m_bufferpos += m_sid.clock(cycles, (short *)m_buffer + m_bufferpos, OUTPUTBUFFERSIZE - m_bufferpos, 1);