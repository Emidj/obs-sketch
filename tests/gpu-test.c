/* Integration test: the actual native DLL + OBS 32.2.2 + Direct3D 11. */
#include <obs.h>
#include <graphics/vec4.h>
#include <sys/stat.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../src/i18n.h"

static uint32_t test_w = 512, test_h = 320;
static const char *input_rgba = NULL;
static const char *output_dir = "tests/artifacts";
struct fixture { gs_texture_t *texture; uint32_t w, h; uint8_t *rgba; };
static struct fixture *active_fixture;
static const char *fixture_name(void *p) { UNUSED_PARAMETER(p); return "Sketch test fixture"; }
static void *fixture_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings); UNUSED_PARAMETER(source);
    struct fixture *f = bzalloc(sizeof(*f));
    f->w = test_w; f->h = test_h;
    f->rgba = bmalloc(f->w*f->h*4);
    for (uint32_t y = 0; y < f->h; y++) for (uint32_t x = 0; x < f->w; x++) {
        size_t i = ((size_t)y*f->w+x)*4;
        float nx = ((float)x/f->w - .5f)*2.f, ny = ((float)y/f->h - .5f)*2.f;
        float r = sqrtf(nx*nx + ny*ny);
        float shade = fmaxf(0.f, 1.f-r*.8f);
        f->rgba[i] = (uint8_t)(40 + 205*shade);
        f->rgba[i+1] = (uint8_t)(35 + 175*shade*shade);
        f->rgba[i+2] = (uint8_t)(30 + 60*shade);
        unsigned a = r < .72f ? 255 : r > .92f ? 0 : (unsigned)(255*(.92f-r)/.2f);
        if (y > f->h*3/4) a = x % 256; // all 256 alpha levels
        if (y > f->h/3 && y < f->h/2 && x > f->w/3 && x < f->w*2/3) {
            f->rgba[i] = 25; f->rgba[i+1] = 80; f->rgba[i+2] = 170;
        }
        f->rgba[i+3] = (uint8_t)a;
    }
    if (input_rgba) {
        FILE *file = fopen(input_rgba,"rb");
        if (!file || fread(f->rgba,1,(size_t)f->w*f->h*4,file) != (size_t)f->w*f->h*4) exit(13);
        fclose(file);
    }
    obs_enter_graphics();
    const uint8_t *pixels = f->rgba;
    f->texture = gs_texture_create(f->w, f->h, GS_RGBA, 1, &pixels, 0);
    obs_leave_graphics();
    active_fixture=f;
    return f;
}
static void fixture_destroy(void *p)
{
    struct fixture *f = p;
    obs_enter_graphics(); gs_texture_destroy(f->texture); obs_leave_graphics();
    bfree(f->rgba); bfree(f);
}
static uint32_t fixture_width(void *p) { return ((struct fixture *)p)->w; }
static uint32_t fixture_height(void *p) { return ((struct fixture *)p)->h; }
static void fixture_render(void *p, gs_effect_t *effect)
{
    struct fixture *f = p;
    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), f->texture);
    gs_draw_sprite(f->texture, 0, f->w, f->h);
}
static struct obs_source_info fixture_info = {
    .id = "sketch_gpu_fixture", .type = OBS_SOURCE_TYPE_INPUT, .output_flags = OBS_SOURCE_VIDEO,
    .get_name = fixture_name, .create = fixture_create, .destroy = fixture_destroy,
    .get_width = fixture_width, .get_height = fixture_height, .video_render = fixture_render
};

static uint8_t *render(obs_source_t *source, uint32_t w, uint32_t h)
{
    uint8_t *result = malloc(w*h*4);
    obs_enter_graphics();
    gs_texrender_t *rt = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    gs_stagesurf_t *stage = gs_stagesurface_create(w,h,GS_RGBA);
    if (!gs_texrender_begin(rt,w,h)) exit(10);
    struct vec4 zero; vec4_zero(&zero);
    gs_clear(GS_CLEAR_COLOR,&zero,0,0);
    gs_ortho(0,(float)w,0,(float)h,-100,100);
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_enable_depth_test(false);
    gs_set_cull_mode(GS_NEITHER);
    gs_set_linear_srgb(false);
    gs_enable_framebuffer_srgb(false);
    gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    obs_source_video_render(source);
    gs_blend_state_pop();
    gs_texrender_end(rt);
    gs_stage_texture(stage,gs_texrender_get_texture(rt));
    uint8_t *mapped = NULL; uint32_t stride = 0;
    if (!gs_stagesurface_map(stage,&mapped,&stride)) exit(11);
    for (uint32_t y = 0; y < h; y++) memcpy(result+y*w*4,mapped+y*stride,w*4);
    gs_stagesurface_unmap(stage);
    gs_stagesurface_destroy(stage); gs_texrender_destroy(rt);
    obs_leave_graphics();
    return result;
}

static int verify_alpha(uint8_t *base, uint8_t *paint, size_t n)
{
    int bad = 0, spill = 0;
    for (size_t i = 0; i < n; i++) {
        if (base[i*4+3] != paint[i*4+3]) bad++;
        if (!paint[i*4+3] && (paint[i*4] || paint[i*4+1] || paint[i*4+2])) spill++;
    }
    printf("alpha_mismatches=%d transparent_RGB_spill=%d\n",bad,spill);
    return bad+spill;
}
static void dump(const char *name, uint8_t *p, size_t n)
{
    char path[2048]; snprintf(path,sizeof(path),"%s/%s",output_dir,name);
    FILE *f = fopen(path,"wb"); if (!f) exit(12); fwrite(p,1,n,f); fclose(f);
}

static void fixture_pan(int dx,int dy)
{
    struct fixture *f=active_fixture;
    uint8_t *pixels=calloc((size_t)f->w*f->h,4);
    for(int y=dy;y<(int)f->h;y++) for(int x=dx;x<(int)f->w;x++)
        memcpy(pixels+((size_t)y*f->w+x)*4,f->rgba+((size_t)(y-dy)*f->w+x-dx)*4,4);
    obs_enter_graphics();
    gs_texture_destroy(f->texture);
    const uint8_t *data=pixels;
    f->texture=gs_texture_create(f->w,f->h,GS_RGBA,1,&data,0);
    obs_leave_graphics(); free(pixels);
}

static int motion_sequence(void)
{
    double errors[2]={0,0};
    int failures=0;
    for(int follow=0;follow<2;follow++) {
        obs_source_t *source=obs_source_create_private("sketch_gpu_fixture","Moving fixture",NULL);
        obs_source_t *filter=obs_source_create_private("obs_sketch_native_v1","Moving sketch",NULL);
        if(!source || !filter) return 1;
        obs_data_t *settings=obs_source_get_settings(filter);
        obs_data_set_int(settings,"style",3);
        obs_data_set_bool(settings,"follow_camera",follow!=0);
        obs_data_set_double(settings,"roughness",0);
        obs_data_set_double(settings,"hatch_density",.65);
        obs_source_update(filter,settings); obs_data_release(settings);
        obs_source_filter_add(source,filter);
        uint8_t *first=NULL;
        for(int step=0;step<=20;step++) {
            int dx=step*3,dy=step;
            fixture_pan(dx,dy); os_sleep_ms(40);
            uint8_t *out=render(source,test_w,test_h);
            char name[128]; snprintf(name,sizeof(name),"motion-%d-%02d.rgba",follow,step); dump(name,out,(size_t)test_w*test_h*4);
            if(!step) { first=malloc((size_t)test_w*test_h*4); memcpy(first,out,(size_t)test_w*test_h*4); }
            if(step==20) {
                size_t count=0;
                for(int y=dy+24;y<(int)test_h-24;y++) for(int x=dx+24;x<(int)test_w-24;x++) {
                    size_t k=((size_t)y*test_w+x)*4,l=((size_t)(y-dy)*test_w+x-dx)*4;
                    if(first[l+3]!=out[k+3]) failures++;
                    errors[follow]+=abs((int)out[k]-(int)first[l]); count++;
                }
                errors[follow]/=count;
            }
            free(out);
        }
        free(first);
        obs_source_filter_remove(source,filter); obs_source_release(filter); obs_source_release(source);
    }
    printf("PAN: fixed_pattern_error=%.4f tracked_pattern_error=%.4f\n",errors[0],errors[1]);
    if(errors[1]>=errors[0]*.65) failures++;
    printf("MOTION GPU RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 7 && argc != 8) { fprintf(stderr,"usage: gpu-test OBS_INSTALL_ROOT PLUGIN_DLL [INPUT_RGBA WIDTH HEIGHT OUTPUT_DIR [--motion]]\n"); return 2; }
    if (argc >= 7) {
        input_rgba = argv[3]; test_w = (uint32_t)atoi(argv[4]); test_h = (uint32_t)atoi(argv[5]); output_dir = argv[6];
        if (!test_w || !test_h || test_w > 8192 || test_h > 8192) return 2;
    }
    char data[2048], graphics[2048];
    snprintf(data,sizeof(data),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if (!obs_startup("en-US",NULL,NULL)) return 3;
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(data); // Compatibility helper for standalone libobs test host.
    #pragma clang diagnostic pop
    struct obs_video_info vi = {
        .graphics_module = graphics, .fps_num = 30, .fps_den = 1,
        .base_width = 512, .base_height = 320, .output_width = 512, .output_height = 320,
        .output_format = VIDEO_FORMAT_RGBA, .adapter = 0, .gpu_conversion = false,
        .colorspace = VIDEO_CS_709, .range = VIDEO_RANGE_FULL, .scale_type = OBS_SCALE_BILINEAR
    };
    if (obs_reset_video(&vi) != OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module = NULL;
    if (obs_open_module(&module,argv[2],".") != MODULE_SUCCESS || !obs_init_module(module)) return 5;
    obs_register_source(&fixture_info);
    if(argc==8) {
        if(strcmp(argv[7],"--motion")) {obs_shutdown();return 2;}
        int result=motion_sequence(); obs_shutdown(); return result?1:0;
    }
    int failures = 0;
    for (int size = 0; size < (input_rgba ? 1 : 2); size++) {
        if (size) { test_w=257; test_h=73; }
        obs_source_t *source = obs_source_create_private("sketch_gpu_fixture","Fixture",NULL);
        if (!source) return 6;
        uint8_t *baseline = render(source,test_w,test_h);
        size_t n = (size_t)test_w*test_h;
        if (!size) dump("original.rgba",baseline,n*4);
        obs_source_t *filter = obs_source_create_private("obs_sketch_native_v1","Sketch",NULL);
        if (!filter) return 7;
        obs_properties_t *props = obs_source_properties(filter);
        if (!props || !obs_properties_get(props,"amount") || !obs_properties_get(props,"style")) return 8;
        if(strcmp(obs_property_description(obs_properties_get(props,"amount")),sketch_text("amount"))) {printf("FAIL: Windows UI language not applied\n");failures++;}
        obs_property_t *last=obs_properties_first(props);
        const char *last_key=NULL;
        while(last) {last_key=obs_property_name(last);obs_property_next(&last);}
        if(!last_key || strcmp(last_key,"credit") || strcmp(obs_property_description(obs_properties_get(props,"credit")),"A Plugin By DjEmiMas")) {
            printf("FAIL: missing or misplaced author credit\n");failures++;
        }
        obs_property_t *logo = obs_properties_get(props,"author_logo");
        if(!logo || !strstr(obs_property_description(logo),"data:image/png;base64,iVBOR")) {
            printf("FAIL: missing embedded author logo\n");failures++;
        }
        if (obs_properties_get(props,"reference_preset") || strcmp(obs_source_get_display_name("obs_sketch_native_v1"),"OBS Sketch")) {
            printf("FAIL: obsolete preset or incorrect plugin name\n"); failures++;
        }
        if(obs_property_list_item_count(obs_properties_get(props,"style"))!=4) {printf("FAIL: expected four finishes\n");failures++;}
        obs_source_filter_add(source,filter);
        obs_data_t *inherited=obs_source_get_settings(filter);
        obs_data_set_double(inherited,"color_reveal",1);
        obs_source_update(filter,inherited); obs_data_release(inherited); os_sleep_ms(100);
        uint8_t *finish_outputs[4]={0};
        for (int mode = 0; mode < 4; mode++) {
            obs_data_t *settings = obs_source_get_settings(filter);
            obs_data_set_int(settings,"style",mode);
            obs_data_set_double(settings,"amount",1.0);
            // Reproduce the reported bug: change ONLY the finish, keeping the
            // saved white-paint value at 1. Never tune each mode in the test.
            obs_data_set_double(settings,"paper",1);
            obs_property_modified(obs_properties_get(props,"style"),settings);
            char description_key[32]; snprintf(description_key,sizeof(description_key),"description_%d",mode);
            if(strcmp(obs_property_description(obs_properties_get(props,"style_description")),sketch_text(description_key))) {printf("FAIL: untranslated finish description\n");failures++;}
            if(obs_data_get_double(settings,"color_reveal")!=0) {printf("FAIL: full color reveal masked new finish\n");failures++;}
            if (obs_property_visible(obs_properties_get(props,"paper")) != (mode==3) ||
                obs_property_visible(obs_properties_get(props,"saturation")) != (mode==0)) {
                printf("FAIL: irrelevant style controls are visible\n"); failures++;
            }
            obs_source_update(filter,settings);
            obs_data_release(settings);
            // Updates for video sources execute on the OBS video thread.
            os_sleep_ms(100);
            uint8_t *out = render(source,test_w,test_h);
            printf("size=%ux%u mode=%d ",test_w,test_h,mode);
            failures += verify_alpha(baseline,out,n);
            if (memcmp(baseline,out,n*4)==0) { printf("FAIL: effect did not alter RGB\n"); failures++; }
            finish_outputs[mode]=malloc(n*4); memcpy(finish_outputs[mode],out,n*4);
            int chromatic=0;
            for(size_t i=0;i<n;i++) if(abs((int)out[i*4]-(int)out[i*4+1])>1 || abs((int)out[i*4]-(int)out[i*4+2])>1) chromatic++;
            if(mode==1 && chromatic) {printf("FAIL: monochrome finish contains tint\n");failures++;}
            if(!input_rgba && (mode==0 || mode==2) && chromatic<100) {printf("FAIL: color finish lost source colors\n");failures++;}
            if(mode==2) {
                settings=obs_source_get_settings(filter);
                obs_data_set_double(settings,"ink",0);
                obs_source_update(filter,settings); os_sleep_ms(100);
                uint8_t *without_ink=render(source,test_w,test_h);
                int error=0;
                for(size_t i=0;i<n*4;i++) {int d=abs((int)without_ink[i]-(int)baseline[i]);if(d>error)error=d;}
                printf("original_color_base_max_error=%d\n",error);
                if(error>1) failures++;
                free(without_ink);
                obs_data_set_double(settings,"ink",1);
                obs_source_update(filter,settings); obs_data_release(settings); os_sleep_ms(100);
            }
            if (mode == 3) {
                int tinted = 0;
                for (size_t i = 0; i < n; i++)
                    if (abs((int)out[i*4]-(int)out[i*4+1]) > 1 || abs((int)out[i*4]-(int)out[i*4+2]) > 1) tinted++;
                printf("white_paint_tinted_pixels=%d\n",tinted);
                failures += tinted != 0;
                if (!input_rgba && !size) {
                    // The central blue panel must become light paint, not dark photographic grey.
                    size_t i = ((size_t)(test_h*5/12)*test_w+test_w/2)*4;
                    if (out[i] < 180) { printf("FAIL: dark material was not repainted white\n"); failures++; }
                }
                uint8_t *repeat = render(source,test_w,test_h);
                if (memcmp(out,repeat,n*4)) { printf("FAIL: static ink changed between frames\n"); failures++; }
                free(repeat);
                obs_data_t *modified = obs_source_get_settings(filter);
                obs_data_set_double(modified,"hatching",0);
                obs_source_update(filter,modified);
                os_sleep_ms(100);
                uint8_t *no_hatch = render(source,test_w,test_h);
                failures += verify_alpha(baseline,no_hatch,n);
                if (!memcmp(out,no_hatch,n*4)) { printf("FAIL: hatching control has no effect\n"); failures++; }
                free(no_hatch);
                obs_data_set_double(modified,"amount",.5);
                obs_source_update(filter,modified);
                obs_data_release(modified);
                os_sleep_ms(100);
                uint8_t *mixed = render(source,test_w,test_h);
                failures += verify_alpha(baseline,mixed,n);
                if (!memcmp(out,mixed,n*4) || !memcmp(baseline,mixed,n*4)) { printf("FAIL: intermediate strength did not blend\n"); failures++; }
                free(mixed);
                if(obs_properties_get(props,"color_reveal")) {
                    modified=obs_source_get_settings(filter);
                    obs_data_set_double(modified,"color_reveal",1);
                    obs_data_set_double(modified,"hatching",.85);
                    obs_data_set_double(modified,"amount",1);
                    obs_source_update(filter,modified); os_sleep_ms(100);
                    uint8_t *color=render(source,test_w,test_h);
                    failures+=verify_alpha(baseline,color,n);
                    int colored=0;
                    for(size_t i=0;i<n;i++) if(abs((int)color[i*4]-(int)color[i*4+2])>8) colored++;
                    if(!input_rgba && !colored) { printf("FAIL: original color not revealed\n");failures++; }
                    if(!memcmp(color,baseline,n*4)) {printf("FAIL: reveal removed ink\n");failures++;}
                    if(!size) dump("color-reveal.rgba",color,n*4);
                    free(color);
                    obs_data_set_double(modified,"ink",0);
                    obs_source_update(filter,modified); obs_data_release(modified); os_sleep_ms(100);
                    color=render(source,test_w,test_h);
                    int max_error=0;
                    for(size_t i=0;i<n*4;i++) {int e=abs((int)color[i]-(int)baseline[i]);if(e>max_error) max_error=e;}
                    printf("reveal_without_ink_max_error=%d\n",max_error);
                    if(max_error>1) failures++;
                    free(color);
                }
            }
            if (!size) { char name[128]; snprintf(name,sizeof(name),"mode-%d.rgba",mode); dump(name,out,n*4); }
            free(out);
        }
        for(int a=0;a<4;a++) for(int b=a+1;b<4;b++) {
            double error=0; size_t samples=0;
            for(size_t i=0;i<n;i++) if(baseline[i*4+3]==255) {
                for(int c=0;c<3;c++) error+=abs((int)finish_outputs[a][i*4+c]-(int)finish_outputs[b][i*4+c]);
                samples+=3;
            }
            error/=samples?samples:1;
            printf("finish_pair=%d,%d mean_RGB_difference=%.3f\n",a,b,error);
            if(error<2) {printf("FAIL: finishes are visually indistinguishable\n");failures++;}
        }
        for(int mode=0;mode<4;mode++) free(finish_outputs[mode]);
        obs_data_t *settings = obs_source_get_settings(filter);
        obs_data_set_double(settings,"amount",0);
        obs_source_update(filter,settings); obs_data_release(settings);
        os_sleep_ms(100);
        uint8_t *bypass = render(source,test_w,test_h);
        if (!size) dump("bypass.rgba",bypass,n*4);
        int mismatch = memcmp(baseline,bypass,n*4)!=0;
        printf("zero_strength_identical=%s\n",mismatch ? "false" : "true"); failures += mismatch;
        free(bypass); free(baseline);
        obs_properties_destroy(props);
        obs_source_filter_remove(source,filter);
        obs_source_release(filter); obs_source_release(source);
    }
    obs_shutdown();
    printf("RESULT: %s (%d failures)\n",failures ? "FAIL" : "PASS",failures);
    return failures ? 1 : 0;
}
