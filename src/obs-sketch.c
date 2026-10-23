/* OBS Sketch 1.5.0 - SPDX-License-Identifier: GPL-2.0-or-later */
#include <obs-module.h>
#include <util/bmem.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include "sketch-shader.h"
#include "motion.h"
#include "i18n.h"
#include "branding.h"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_name(void) { return "OBS Sketch"; }
MODULE_EXPORT const char *obs_module_description(void) { return sketch_text("module_description"); }
MODULE_EXPORT const char *obs_module_author(void) { return "OBS Sketch contributors"; }

struct control { const char *key, *label; double min, max, step, initial; };
static const struct control controls[] = {
    {"amount", "Intensidad del efecto", 0, 1, .01, 1},
    {"line_width", "Grosor del trazo (px de la fuente)", .5, 8, .1, 2.4},
    {"edge_threshold", "Umbral de contornos (mayor = menos detalle)", .02, .7, .01, .30},
    {"ink", "Intensidad de tinta", 0, 1, .01, 1},
    {"levels", "Niveles de sombreado", 2, 12, 1, 3},
    {"smoothing", "Suavizado de textura", 0, 1, .01, .9},
    {"saturation", "Saturación", 0, 2, .01, 1.15},
    {"paper", "Pintura blanca", 0, 1, .01, 1},
    {"hatching", "Rayado de sombras", 0, 1, .01, .85},
    {"hatch_spacing", "Separación del rayado (px)", 4, 32, .5, 11},
    {"roughness", "Irregularidad del trazo", 0, 1, .01, .4},
    {"highlights", "Acentos blancos", 0, 1, .01, .35},
    {"silhouette", "Contorno interior de la silueta", 0, 1, .01, 1},
    {"paint_shadows", "Gris de las sombras sobre pintura blanca", 0, .6, .01, .20},
    {"fine_lines", "Trazos finos en hendiduras", 0, 1, .01, .40},
    {"hatch_length", "Longitud de grupos de rayas (px)", 8, 160, 1, 48},
    {"black_fill", "Negro en huecos profundos", 0, 1, .01, .85},
    {"hatch_density", "Cantidad de grupos de rayas", 0, 1, .01, .35},
    {"color_reveal", "Revelar color original (conservar tinta)", 0, 1, .01, 0},
};
#define NCONTROLS (sizeof(controls) / sizeof(controls[0]))

struct sketch {
    obs_source_t *source;
    gs_effect_t *effect;
    gs_eparam_t *params[NCONTROLS], *width, *height, *style;
    float values[NCONTROLS], style_value;
    gs_eparam_t *motion_matrix, *motion_offset, *tracking_size;
    gs_texrender_t *tracking_render;
    gs_stagesurf_t *tracking_stage;
    struct motion motion;
    uint8_t tracking_pixels[MOTION_MAX*MOTION_MAX*4];
    uint8_t tracking_smooth[MOTION_MAX*MOTION_MAX*4];
    uint32_t tracking_w, tracking_h, source_w, source_h;
    uint64_t last_track_time;
    bool follow_camera;
    int64_t reset_serial;
};

static const char *sketch_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "OBS Sketch";
}

static void sketch_defaults(obs_data_t *settings)
{
    for (size_t i = 0; i < NCONTROLS; i++)
        obs_data_set_default_double(settings, controls[i].key, controls[i].initial);
    obs_data_set_default_int(settings, "style", 3);
    obs_data_set_default_bool(settings, "follow_camera", true);
}

static int64_t prepare_style(obs_data_t *settings)
{
    int64_t style = obs_data_get_int(settings,"style");
    if (style < 0 || style > 3) style = 3;
    // Revealing all original color deliberately bypasses a finish's base. On a
    // new selection, show that finish first; retain other user adjustments.
    if (obs_data_has_user_value(settings,"active_finish") && obs_data_get_int(settings,"active_finish") != style)
        obs_data_set_double(settings,"color_reveal",0);
    obs_data_set_int(settings,"active_finish",style);
    return style;
}

static void sketch_update(void *ptr, obs_data_t *settings)
{
    struct sketch *s = ptr;
    int64_t style = prepare_style(settings);
    for (size_t i = 0; i < NCONTROLS; i++) {
        double v = obs_data_get_double(settings, controls[i].key);
        // Reject NaN too: malformed imported settings must not reach shaders.
        if (!(v >= controls[i].min)) v = controls[i].min;
        if (v > controls[i].max) v = controls[i].max;
        s->values[i] = (float)v;
    }
    s->style_value = (float)style;
    bool follow = obs_data_get_bool(settings, "follow_camera");
    int64_t serial = obs_data_get_int(settings, "tracking_reset");
    if (follow != s->follow_camera || serial != s->reset_serial) motion_reset(&s->motion);
    s->follow_camera = follow; s->reset_serial = serial;
}

static void sketch_destroy(void *ptr)
{
    struct sketch *s = ptr;
    if (!s) return;
    obs_enter_graphics();
    gs_effect_destroy(s->effect);
    gs_texrender_destroy(s->tracking_render);
    gs_stagesurface_destroy(s->tracking_stage);
    obs_leave_graphics();
    bfree(s);
}

static void *sketch_create(obs_data_t *settings, obs_source_t *source)
{
    struct sketch *s = bzalloc(sizeof(*s));
    s->source = source;
    char *error = NULL;
    obs_enter_graphics();
    s->effect = gs_effect_create(sketch_shader, "obs-sketch.effect", &error);
    bool valid = s->effect != NULL;
    if (s->effect) {
        for (size_t i = 0; i < NCONTROLS; i++) {
            s->params[i] = gs_effect_get_param_by_name(s->effect, controls[i].key);
            valid = valid && s->params[i] != NULL;
        }
        s->width = gs_effect_get_param_by_name(s->effect, "texture_width");
        s->height = gs_effect_get_param_by_name(s->effect, "texture_height");
        s->style = gs_effect_get_param_by_name(s->effect, "style");
        valid = valid && s->width && s->height && s->style;
        s->motion_matrix = gs_effect_get_param_by_name(s->effect, "motion_matrix");
        s->motion_offset = gs_effect_get_param_by_name(s->effect, "motion_offset");
        s->tracking_size = gs_effect_get_param_by_name(s->effect, "tracking_size");
        valid = valid && s->motion_matrix && s->motion_offset && s->tracking_size;
    }
    obs_leave_graphics();
    if (!valid) blog(LOG_ERROR, "[obs-sketch] Shader initialization failed: %s", error ? error : "missing parameter");
    bfree(error);
    if (!valid) { sketch_destroy(s); return NULL; }
    sketch_update(s, settings);
    return s;
}

static bool style_controls(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(property);
    int64_t style = prepare_style(settings);
    bool marker = style == 3;
    const char *keys[] = {"paper", "paint_shadows", "fine_lines", "hatch_length", "black_fill", "hatch_density"};
    for (size_t i = 0; i < OBS_COUNTOF(keys); i++)
        obs_property_set_visible(obs_properties_get(props, keys[i]), marker);
    obs_property_set_visible(obs_properties_get(props, "saturation"), style == 0);
    const char *base_controls[] = {"levels", "smoothing", "highlights", "color_reveal"};
    for (size_t i = 0; i < OBS_COUNTOF(base_controls); i++)
        obs_property_set_visible(obs_properties_get(props, base_controls[i]), style != 2);
    const char *descriptions[] = {
        "description_0", "description_1", "description_2", "description_3"
    };
    obs_property_set_description(obs_properties_get(props,"style_description"), sketch_text(descriptions[style >= 0 && style <= 3 ? style : 3]));
    return true;
}

static bool reset_tracking(obs_properties_t *props, obs_property_t *property, void *ptr)
{
    UNUSED_PARAMETER(props); UNUSED_PARAMETER(property);
    struct sketch *s = ptr;
    if (!s) return false;
    obs_data_t *settings = obs_source_get_settings(s->source);
    obs_data_set_int(settings, "tracking_reset", obs_data_get_int(settings,"tracking_reset")+1);
    obs_source_update(s->source,settings); obs_data_release(settings);
    return false;
}

static obs_properties_t *sketch_properties(void *ptr)
{
    obs_properties_t *p = obs_properties_create();
    obs_property_t *style = obs_properties_add_list(p, "style", sketch_text("style"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(style, sketch_text("style_3"), 3);
    obs_property_list_add_int(style, sketch_text("style_0"), 0);
    obs_property_list_add_int(style, sketch_text("style_1"), 1);
    obs_property_list_add_int(style, sketch_text("style_2"), 2);
    obs_properties_add_text(p, "style_description", sketch_text("description_3"), OBS_TEXT_INFO);
    for (size_t i = 0; i < NCONTROLS; i++)
        obs_properties_add_float_slider(p, controls[i].key, sketch_text(controls[i].key),
                                        controls[i].min, controls[i].max, controls[i].step);
    obs_property_set_modified_callback(style, style_controls);
    obs_properties_add_bool(p, "follow_camera", sketch_text("follow_camera"));
    obs_properties_add_button2(p, "reset_tracking", sketch_text("reset_tracking"), reset_tracking, ptr);
    obs_properties_add_text(p, "note", sketch_text("note"), OBS_TEXT_INFO);
    obs_properties_add_text(p, "author_logo", sketch_logo_html, OBS_TEXT_INFO);
    obs_properties_add_text(p, "credit", "A Plugin By DjEmiMas", OBS_TEXT_INFO);
    return p;
}

static enum gs_color_space target_space(struct sketch *s)
{
    const enum gs_color_space spaces[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};
    obs_source_t *target = obs_filter_get_target(s->source);
    return target ? obs_source_get_color_space(target, OBS_COUNTOF(spaces), spaces) : GS_CS_SRGB;
}

static enum gs_color_space sketch_color_space(void *ptr, size_t count, const enum gs_color_space *spaces)
{
    UNUSED_PARAMETER(count); UNUSED_PARAMETER(spaces);
    return target_space(ptr);
}

static void track_camera(struct sketch *s, uint32_t width, uint32_t height)
{
    uint32_t longest = width > height ? width : height;
    uint32_t w = width*MOTION_MAX/longest, h = height*MOTION_MAX/longest;
    uint64_t now = obs_get_video_frame_time();
    if (width!=s->source_w || height!=s->source_h || now-s->last_track_time>500000000ULL)
        motion_reset(&s->motion);
    s->source_w=width; s->source_h=height; s->last_track_time=now;
    if (w<32 || h<32) { motion_reset(&s->motion); return; }
    if (!s->tracking_render) s->tracking_render=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
    if (w!=s->tracking_w || h!=s->tracking_h || !s->tracking_stage) {
        gs_stagesurface_destroy(s->tracking_stage);
        s->tracking_stage=gs_stagesurface_create(w,h,GS_RGBA);
        s->tracking_w=w; s->tracking_h=h;
    }
    if (!s->tracking_render || !s->tracking_stage) { motion_reset(&s->motion); return; }
    gs_texrender_reset(s->tracking_render);
    if (!gs_texrender_begin(s->tracking_render,w,h)) { motion_reset(&s->motion); return; }
    struct vec4 clear; vec4_zero(&clear);
    gs_clear(GS_CLEAR_COLOR,&clear,0,0);
    gs_ortho(0,(float)w,0,(float)h,-100,100);
    gs_blend_state_push(); gs_enable_blending(false);
    struct vec2 size; vec2_set(&size,(float)w,(float)h);
    gs_effect_set_vec2(s->tracking_size,&size);
    // OBS retains its input texture; both passes read the same current frame.
    obs_source_process_filter_tech_end(s->source,s->effect,w,h,"Track");
    gs_blend_state_pop(); gs_texrender_end(s->tracking_render);
    gs_stage_texture(s->tracking_stage,gs_texrender_get_texture(s->tracking_render));
    uint8_t *pixels=NULL; uint32_t stride=0;
    if (!gs_stagesurface_map(s->tracking_stage,&pixels,&stride)) { motion_reset(&s->motion); return; }
    for(uint32_t y=0;y<h;y++) memcpy(s->tracking_pixels+y*w*4,pixels+y*stride,w*4);
    gs_stagesurface_unmap(s->tracking_stage);
    // Blur after downsampling for well-behaved subpixel image gradients.
    const int weights[5]={1,4,6,4,1};
    for(int y=0;y<(int)h;y++) for(int x=0;x<(int)w;x++) {
        int sum=0;
        for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++) {
            int xx=x+i,yy=y+j;
            if(xx<0)xx=0; if(xx>=(int)w)xx=(int)w-1;
            if(yy<0)yy=0; if(yy>=(int)h)yy=(int)h-1;
            sum+=s->tracking_pixels[(yy*w+xx)*4]*weights[i+2]*weights[j+2];
        }
        size_t k=((size_t)y*w+x)*4;
        s->tracking_smooth[k]=s->tracking_smooth[k+1]=s->tracking_smooth[k+2]=(uint8_t)((sum+128)/256);
        s->tracking_smooth[k+3]=s->tracking_pixels[k+3];
    }
    motion_update(&s->motion,s->tracking_smooth,(int)w,(int)h);
}

static void set_pattern_transform(struct sketch *s, uint32_t width, uint32_t height)
{
    struct vec4 matrix; vec4_set(&matrix,1,0,0,1);
    struct vec2 offset; vec2_zero(&offset);
    if(s->follow_camera && s->motion.ready) {
        struct motion *m=&s->motion;
        double norm=m->a*m->a+m->b*m->b;
        double a=m->a/norm,b=m->b/norm;
        double sx=(double)width/m->w,sy=(double)height/m->h;
        vec4_set(&matrix,(float)a,(float)(b*sx/sy),(float)(-b*sy/sx),(float)a);
        vec2_set(&offset,(float)((-a*m->tx-b*m->ty)*sx),(float)((b*m->tx-a*m->ty)*sy));
    }
    gs_effect_set_vec4(s->motion_matrix,&matrix);
    gs_effect_set_vec2(s->motion_offset,&offset);
}

static void sketch_render(void *ptr, gs_effect_t *unused)
{
    UNUSED_PARAMETER(unused);
    struct sketch *s = ptr;
    obs_source_t *target = obs_filter_get_target(s->source);
    if (!target) return;
    enum gs_color_space space = target_space(s);
    if (!s->effect || s->values[0] == 0 || space == GS_CS_709_EXTENDED || space == GS_CS_709_SCRGB) {
        motion_reset(&s->motion);
        obs_source_skip_video_filter(s->source);
        return;
    }
    uint32_t width = obs_source_get_base_width(target), height = obs_source_get_base_height(target);
    if (!width || !height) { obs_source_skip_video_filter(s->source); return; }
    // No direct render: normalize all source types into a premultiplied texture.
    enum gs_color_format format = space == GS_CS_SRGB_16F ? GS_RGBA16F : GS_RGBA;
    if (!obs_source_process_filter_begin_with_color_space(s->source, format, space, OBS_NO_DIRECT_RENDERING)) return;
    if(s->follow_camera) track_camera(s,width,height);
    else motion_reset(&s->motion);
    // Ending Track clears effect parameter values; bind Draw's values afterwards.
    gs_effect_set_float(s->width, (float)width);
    gs_effect_set_float(s->height, (float)height);
    gs_effect_set_float(s->style, s->style_value);
    for (size_t i = 0; i < NCONTROLS; i++) gs_effect_set_float(s->params[i], s->values[i]);
    set_pattern_transform(s,width,height);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    obs_source_process_filter_end(s->source, s->effect, width, height);
    gs_blend_state_pop();
}

static struct obs_source_info sketch_info = {
    .id = "obs_sketch_native_v1",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = sketch_name,
    .create = sketch_create,
    .destroy = sketch_destroy,
    .get_defaults = sketch_defaults,
    .get_properties = sketch_properties,
    .update = sketch_update,
    .video_render = sketch_render,
    .video_get_color_space = sketch_color_space,
};

bool obs_module_load(void)
{
    obs_register_source(&sketch_info);
    blog(LOG_INFO, "[obs-sketch] 1.5.0 loaded (Windows display language index=%u)", sketch_language_for_id(GetUserDefaultUILanguage()));
    return true;
}
