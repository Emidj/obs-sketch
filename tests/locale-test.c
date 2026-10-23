/* Test actual Windows-language selection without changing the OS language. */
#include <windows.h>
#include <stdio.h>
static LANGID test_language;
static LANGID simulated_ui_language(void) { return test_language; }
#define GetUserDefaultUILanguage simulated_ui_language
#include "../src/i18n.h"

int main(void)
{
    const struct { unsigned id, expected; } cases[] = {
        {0x0409,0},{0x0809,0},{0x1009,0}, {0x040a,1},{0x0c0a,1},{0x080a,1},
        {0x0410,2},{0x0810,2}, {0x040c,3},{0x080c,3},{0x0c0c,3},
        {0x0407,4},{0x0807,4},{0x0c07,4}, {0x0411,0},{0x0416,0},{0,0}
    };
    const char *labels[]={"Effect strength","Intensidad del efecto","Intensità dell'effetto","Intensité de l'effet","Effektstärke"};
    int failures=0;
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        test_language=(LANGID)cases[i].id;
        if(sketch_language_for_id(test_language)!=cases[i].expected || strcmp(sketch_text("amount"),labels[cases[i].expected])) failures++;
        for(size_t j=0;j<sizeof(sketch_translations)/sizeof(sketch_translations[0]);j++) {
            const char *key=sketch_translations[j].key,*value=sketch_text(key);
            if(!value[0] || !strcmp(key,value)) {printf("Missing translation: %s\n",key);failures++;}
        }
    }
    printf("LOCALE RESULT: %s (17 regional/fallback cases, all plugin strings; %d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
