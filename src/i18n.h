/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SKETCH_I18N_H
#define SKETCH_I18N_H
#include <windows.h>
#include <string.h>
#include "translations.h"

// Follow the signed-in user's Windows DISPLAY language, not number/date format
// or the locale selected by OBS. Regional variants share their base language.
static unsigned sketch_language_for_id(unsigned id)
{
    switch (id & 0x3ff) {
    case 0x0a: return 1; /* es */
    case 0x10: return 2; /* it */
    case 0x0c: return 3; /* fr */
    case 0x07: return 4; /* de */
    default: return 0; /* en and unsupported languages */
    }
}
static const char *sketch_text(const char *key)
{
    unsigned lang=sketch_language_for_id(GetUserDefaultUILanguage());
    for (size_t i=0;i<sizeof(sketch_translations)/sizeof(sketch_translations[0]);i++)
        if (!strcmp(key,sketch_translations[i].key)) return sketch_translations[i].text[lang];
    return key;
}
#endif
