#ifndef SEGMENTS_H
#define SEGMENTS_H
#define LED_A 0x01
#define LED_B 0x02
#define LED_C 0x04
#define LED_D 0x08
#define LED_E 0x10
#define LED_F 0x20
#define LED_G 0x40
#define LED_DP 0x80

static const uint8_t seg_codes[]={ /* 0 */ LED_A|LED_B|LED_C|LED_D|LED_E|LED_F,
                            /* 1 */ LED_B|LED_C,
                            /* 2 */ LED_A|LED_B|LED_D|LED_E|LED_G,
                            /* 3 */ LED_A|LED_B|LED_C|LED_D|LED_G,
                            /* 4 */ LED_B|LED_C|LED_F|LED_G,
                            /* 5 */ LED_A|LED_C|LED_D|LED_F|LED_G,
                            /* 6 */ LED_A|LED_C|LED_D|LED_E|LED_F|LED_G,
                            /* 7 */ LED_A|LED_B|LED_C,
                            /* 8 */ LED_A|LED_B|LED_C|LED_D|LED_E|LED_F|LED_G,
                            /* 9 */ LED_A|LED_B|LED_C|LED_D|LED_F|LED_G,
                            /* A */ LED_A|LED_B|LED_C|LED_G|LED_E|LED_F,
                            /* b */ LED_B|LED_D|LED_E|LED_F|LED_G,
                            /* C */ LED_A|LED_D|LED_E|LED_F,
                            /* d */ LED_B|LED_C|LED_D|LED_E|LED_G,
                            /* E */ LED_A|LED_D|LED_E|LED_F|LED_G,
                            /* F */ LED_A|LED_E|LED_F|LED_G,
                          };

#endif // SEGMENTS_H
