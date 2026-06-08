/*
Parse serial commands into tokens e.g. "/0/pwm 252" into "/0/pwm\0" and "252\0"
Copyright (C) 2019 Ronald Sutherland

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

https://en.wikipedia.org/wiki/BSD_licenses#0-clause_license_(%22Zero_Clause_BSD%22)
*/
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "parse_huart1.h"

// used to assemble command line
char command_buf[COMMAND_BUFFER_SIZE];
uint8_t command_head;
uint8_t command_done;

// used to convert command line into its parts
char *command;
char *arg[MAX_ARGUMENT_COUNT];
uint8_t arg_count;

// set when command_buf address field matches this device's address
uint8_t echo_on;

void initCommandBuffer(void)
{
    command_buf[1] = '\0';  // best to set the address as a null value
    for (uint8_t i = 0; i < MAX_ARGUMENT_COUNT; i++)
    {
        arg[i] = NULL;
    }
    command = NULL;
    command_done = 0;
    command_head = 0;
    arg_count = 0;
    echo_on = 0;
}

// Set echo_on if the address field in command_buf matches this device's address.
// echo_on gates all response output so only the addressed device replies on the RS485 bus.
void CheckAddress(char address)
{
    if ((command_buf[0] == '/') && (command_buf[1] == address))
        echo_on = 1;
    else
        echo_on = 0;
}

// Copy bytes received via DMA into command_buf and signal command_done.
// Stops at the first \r or \n, or when the buffer is full.
void LoadCommandFromDMA(uint8_t *buf, uint16_t size)
{
    initCommandBuffer();
    for (uint16_t i = 0; i < size && command_head < (COMMAND_BUFFER_SIZE - 1); i++)
    {
        if (buf[i] == '\r' || buf[i] == '\n')
            break;
        command_buf[command_head++] = (char)buf[i];
    }
    command_buf[command_head] = '\0';
    command_done = 1;
}

// find argument(s) starting from a given offset
uint8_t findArgument(uint8_t at_command_buf_offset)
{
    if (at_command_buf_offset < COMMAND_BUFFER_SIZE)
    {
        uint8_t lastAlphaNum = at_command_buf_offset;

        // get past any white space, but not end of line (EOL was replaced with a null)
        while (isspace((unsigned char)command_buf[lastAlphaNum]) && !(command_buf[lastAlphaNum] == '\0'))
        {
            lastAlphaNum++;
        }

        // after command+space but the char is null
        if (command_buf[lastAlphaNum] == '\0')
        {
            if (echo_on) printf("{\"err\": \"NullArgAftrCmd+Sp\"}\r\n");
            initCommandBuffer();
            return 0;
        }

        // for each valid argument add it to the arg array of strings
        for (arg_count = 0; command_buf[lastAlphaNum] != '\0'; arg_count++)
        {
            // too many arguments
            if (!(arg_count < MAX_ARGUMENT_COUNT))
            {
                if (echo_on) printf("{\"err\": \"ArgCnt%dAt%d\"}\r\n", arg_count, lastAlphaNum);
                initCommandBuffer();
                return 0;
            }

            arg[arg_count] = command_buf + lastAlphaNum;

            // skip through the argument
            while ((isalnum((unsigned char)command_buf[lastAlphaNum]) || (command_buf[lastAlphaNum] == '-'))
                   && (lastAlphaNum < (COMMAND_BUFFER_SIZE - 1)))
            {
                lastAlphaNum++;
            }
            if (command_buf[lastAlphaNum] == ARGUMNT_DELIMITER)
            {
                if (lastAlphaNum < (COMMAND_BUFFER_SIZE - 2))
                {
                    // check if char after delimiter is valid for an arg
                    if (!(isalnum((unsigned char)command_buf[lastAlphaNum + 1]) || (command_buf[lastAlphaNum + 1] == '-')))
                    {
                        if (echo_on) printf("{\"err\": \"ArgAftr'%c@%d!Valid\"}\r\n",
                                            command_buf[lastAlphaNum], lastAlphaNum);
                        initCommandBuffer();
                        return 0;
                    }

                    // null terminate the argument, e.g. replace the delimiter
                    command_buf[lastAlphaNum] = '\0';
                    lastAlphaNum++;
                }
                else
                {
                    // a delimiter was found but there is not enough room for an argument and null termination
                    if (echo_on) printf("{\"err\": \"DropArgCmdLn2Lng\"}\r\n");
                    initCommandBuffer();
                    return 0;
                }
            }
            // only EOL or delimiter is a valid way to terminate an argument
            else if (command_buf[lastAlphaNum] != '\0')
            {
                if (echo_on) printf("{\"err\": \"!DelimAftrArg'%c@%d\"}\r\n",
                                    command_buf[lastAlphaNum], lastAlphaNum);
                initCommandBuffer();
                return 0;
            }
        }
        return arg_count;
    }
    else
    {
        // do not index past command buffer
        if (echo_on) printf("{\"err\": \"ArgIndxPastCmdBuf\"}\r\n");
        initCommandBuffer();
        return 0;
    }
}

// White space is not allowed before the command.
// Command always starts at position 2 and ends at the first white space.
// The combined address and command looks like an MQTT topic or a file system path,
// e.g. /0/pwm 127
// Find end of command and place a null termination so it can be used as a string.
uint8_t findCommand(void)
{
    uint8_t lastAlpha = 2;
    // if command_buf has "/1/i1scan?",
    // then command_buf[0] is '/' and command_buf[1] is '1', they are used for addressing

    // the command always starts after the address at position 2
    command = command_buf + lastAlpha;

    // Only an isspace or null may terminate a valid command.
    // command_buf[2] must be '/', command_buf[3] must be isalpha, followed by isalnum or '?'.
    while (!(isspace((unsigned char)command_buf[lastAlpha]) || (command_buf[lastAlpha] == '\0'))
           && lastAlpha < (COMMAND_BUFFER_SIZE - 1))
    {
        if ((lastAlpha == 2) && (command_buf[lastAlpha] == '/'))
        {
            lastAlpha++;
        }
        else if ((lastAlpha == 3) && isalpha((unsigned char)command_buf[lastAlpha]))
        {
            lastAlpha++;
        }
        else if ((lastAlpha > 3) && (isalnum((unsigned char)command_buf[lastAlpha]) || (command_buf[lastAlpha] == '?')))
        {
            lastAlpha++;
        }
        else
        {
            if (echo_on) printf("{\"err\": \"BadCharInCmd '%c'\"}\r\n", command_buf[lastAlpha]);
            initCommandBuffer();
            return 0;
        }
    }

    // command does not fit in buffer
    if (lastAlpha >= (COMMAND_BUFFER_SIZE - 1))
    {
        if (echo_on) printf("{\"err\": \"HugeCmd\"}\r\n");
        initCommandBuffer();
        return 0;
    }

    if (isspace((unsigned char)command_buf[lastAlpha]))
    {
        // the next position may be an argument
        if (findArgument(lastAlpha + 1))
        {
            // replace the space with a null so command works as a null terminated string
            command_buf[lastAlpha] = '\0';
        }
        else
        {
            // isspace() found after command but argument was not valid
            if (echo_on) printf("{\"err\": \"CharAftrCmdBad '%c'\"}\r\n", command_buf[lastAlpha + 1]);
            initCommandBuffer();
            return 0;
        }
    }
    else
    {
        if (command_buf[lastAlpha] != '\0')
        {
            // null must end command
            if (echo_on) printf("{\"err\": \"MissNullAftrCmd '%c'\"}\r\n", command_buf[lastAlpha]);
            initCommandBuffer();
            return 0;
        }
    }
    // zero indexing is also the count and should match with strlen()
    return lastAlpha;
}

unsigned long is_arg_in_ul_range(uint8_t arg_num, unsigned long min, unsigned long max)
{
    if (!isdigit((unsigned char)arg[arg_num][0]))
    {
        printf("{\"err\":\"%sArg%d_NaN\"}\r\n", command, arg_num);
        return 0;
    }
    unsigned long ul = strtoul(arg[arg_num], (char **)NULL, 10);
    if ((ul < min) || (ul > max))
    {
        printf("{\"err\":\"%sArg%d_OutOfRng\"}\r\n", command, arg_num);
        return 0;
    }
    return ul;
}

// return arg[arg_num] value if in range
uint8_t is_arg_in_uint8_range(uint8_t arg_num, uint8_t min, uint8_t max)
{
    if (!isdigit((unsigned char)arg[arg_num][0]))
    {
        printf("{\"err\":\"%sArg%d_NaN\"}\r\n", command, arg_num);
        return 0;
    }
    uint8_t argument = (uint8_t)atoi(arg[arg_num]);
    if ((argument < min) || (argument > max))
    {
        printf("{\"err\":\"%sArg%d_OutOfRng\"}\r\n", command, arg_num);
        return 0;
    }
    return argument;
}
