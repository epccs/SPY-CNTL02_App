#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "parse_huart1.h"
#include "id.h"

// /id?            -> {"id":{"name":"...","desc":"...","gcc":"..."}}
// /id? name       -> {"id":{"name":"..."}}
// /id? desc       -> {"id":{"desc":"..."}}
// /id? gcc        -> {"id":{"gcc":"..."}}
void Id(const char name[])
{
    if (arg_count == 0)
    {
        printf("{\"id\":{\"name\":\"%s\",\"desc\":\"SPY-CNTL02 App (STM32C092KCT6)\",\"gcc\":\"%s\"}}\r\n", name, __VERSION__);
    }
    else if ((arg_count == 1) && (strcmp(arg[0], "name") == 0))
    {
        printf("{\"id\":{\"name\":\"%s\"}}\r\n", name);
    }
    else if ((arg_count == 1) && (strcmp(arg[0], "desc") == 0))
    {
        printf("{\"id\":{\"desc\":\"SPY-CNTL02 App (STM32C092KCT6)\"}}\r\n");
    }
    else if ((arg_count == 1) && (strcmp(arg[0], "gcc") == 0))
    {
        printf("{\"id\":{\"gcc\":\"%s\"}}\r\n", __VERSION__);
    }
    else
    {
        printf("{\"err\":\"idBadArg_%s\"}\r\n", arg[0]);
    }
}
