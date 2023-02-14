#include "harfbuzz/src/harfbuzz.cc"

HB_BEGIN_DECLS

int
hbjs_glyph_svg (hb_font_t *font, hb_codepoint_t glyph, char *buf, unsigned buf_size);

int
hbjs_glyph_blob (hb_font_t *font, hb_codepoint_t glyph, char *buf, unsigned buf_size, unsigned units_per_em);

void
hbjs_set_weight (hb_font_t *font, unsigned weight);

unsigned
hbjs_shape_with_trace (hb_font_t *font, hb_buffer_t* buf,
                       char* featurestring,
                       unsigned int stop_at, unsigned int stop_phase,
                       char *outbuf, unsigned buf_size);

void *free_ptr(void);

HB_END_DECLS


void *free_ptr(void) { return (void *) free; }

enum {
  HB_SHAPE_DONT_STOP,
  HB_SHAPE_GSUB_PHASE,
  HB_SHAPE_GPOS_PHASE
};

struct user_data_t {
  user_data_t(char *str_,
              unsigned size_,
              unsigned units_per_em_ = 0,
              unsigned stop_at_ = 0,
              unsigned stop_phase_ = 0)
    : str(str_)
    , size(size_)
    , units_per_em(units_per_em_)
    , stop_at(stop_at_)
    , stop_phase(stop_phase_)
  {}
  char *str = nullptr;
  unsigned units_per_em = 0;
  unsigned size = 0;
  unsigned consumed = 0;
  hb_bool_t failure = false;
  unsigned stop_at = 0;
  unsigned stop_phase = 0;
  hb_bool_t stopping = false;
  unsigned current_phase = 0;
};


static void
_user_data_printf (user_data_t *data, const char *format, ...)
{
#define BUFSIZE 1000
  char buf[BUFSIZE];
  int len;
  va_list va;

  if (!data || data->failure)
    return;

  va_start(va, format);
  len = vsnprintf(buf, BUFSIZE, format, va);
  va_end(va);

  if (data->consumed + len >= data->size || len < 0 || len > BUFSIZE)
  {
      data->failure = true;
      return;
  }

  memcpy (data->str + data->consumed, buf, len);
  data->consumed += len;
#undef BUFSIZE
}

static void
move_to (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	 float to_x, float to_y,
	 void *)
{
  _user_data_printf (draw_data, "M%g,%g", (double)to_x, (double)to_y);
}

static void
line_to (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	 float to_x, float to_y,
	 void *)
{
  _user_data_printf (draw_data, "L%g,%g", (double)to_x, (double)to_y);
}

static void
quadratic_to (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	      float control_x, float control_y,
	      float to_x, float to_y,
	      void *)
{
  _user_data_printf (draw_data, "Q%g,%g %g,%g",
                     (double)control_x,
                     (double)control_y,
                     (double)to_x,
                     (double)to_y);
}

static void
cubic_to (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	  float control1_x, float control1_y,
	  float control2_x, float control2_y,
	  float to_x, float to_y,
	  void *)
{
  _user_data_printf (draw_data, "C%g,%g %g,%g %g,%g",
                     (double)control1_x,
                     (double)control1_y,
                     (double)control2_x,
                     (double)control2_y,
                     (double)to_x,
                     (double)to_y);
}

static void
close_path (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *, void *)
{
  _user_data_printf (draw_data, "Z");
}

static hb_draw_funcs_t *funcs = 0;

int
hbjs_glyph_svg (hb_font_t *font, hb_codepoint_t glyph, char *buf, unsigned buf_size)
{
  if (funcs == 0) /* not the best pattern for multi-threaded apps which is not a concern here */
  {
    funcs = hb_draw_funcs_create (); /* will be leaked */
    hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to, nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func (funcs, (hb_draw_quadratic_to_func_t) quadratic_to, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to, nullptr, nullptr);
    hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path, nullptr, nullptr);
  }

  user_data_t draw_data(buf, buf_size);
  hb_font_get_glyph_shape (font, glyph, funcs, &draw_data);
  if (draw_data.failure)
    return -1;

  buf[draw_data.consumed] = '\0';
  return draw_data.consumed;
}

static hb_draw_funcs_t *blob_funcs = 0;

static void
move_to_blob (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	 float to_x, float to_y,
	 void *)
{
  if (draw_data->consumed + 9 >= draw_data->size)
  {
      draw_data->failure = true;
      return;
  }

  draw_data->str[draw_data->consumed++] = 1;

  float to_x_adjusted = to_x/(draw_data->units_per_em);
  float to_y_adjusted = to_y/(draw_data->units_per_em);

  memcpy(&draw_data->str[draw_data->consumed], &to_x_adjusted, sizeof(float));
  draw_data->consumed += 4;
  memcpy(&draw_data->str[draw_data->consumed], &to_y_adjusted, sizeof(float));
  draw_data->consumed += 4;
}

static void
line_to_blob (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	 float to_x, float to_y,
	 void *)
{
  if (draw_data->consumed + 9 >= draw_data->size)
  {
      draw_data->failure = true;
      return;
  }

  draw_data->str[draw_data->consumed++] = 2;

  float to_x_adjusted = to_x/(draw_data->units_per_em);
  float to_y_adjusted = to_y/(draw_data->units_per_em);

  memcpy(&draw_data->str[draw_data->consumed], &to_x_adjusted, sizeof(float));
  draw_data->consumed += 4;
  memcpy(&draw_data->str[draw_data->consumed], &to_y_adjusted, sizeof(float));
  draw_data->consumed += 4;
}

static void
quadratic_to_blob (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *,
	      float control_x, float control_y,
	      float to_x, float to_y,
	      void *)
{
  if (draw_data->consumed + 17 >= draw_data->size)
  {
      draw_data->failure = true;
      return;
  }

  draw_data->str[draw_data->consumed++] = 3;

  float control_x_adjusted = control_x/(draw_data->units_per_em);
  float control_y_adjusted = control_y/(draw_data->units_per_em);

  memcpy(&draw_data->str[draw_data->consumed], &control_x_adjusted, sizeof(float));
  draw_data->consumed += 4;
  memcpy(&draw_data->str[draw_data->consumed], &control_y_adjusted, sizeof(float));
  draw_data->consumed += 4;

  float to_x_adjusted = to_x/(draw_data->units_per_em);
  float to_y_adjusted = to_y/(draw_data->units_per_em);

  memcpy(&draw_data->str[draw_data->consumed], &to_x_adjusted, sizeof(float));
  draw_data->consumed += 4;
  memcpy(&draw_data->str[draw_data->consumed], &to_y_adjusted, sizeof(float));
  draw_data->consumed += 4;
}

static void
cubic_to_blob (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *current_state,
	  float control1_x, float control1_y,
	  float control2_x, float control2_y,
	  float to_x, float to_y,
	  void *)
{
  float current_x = current_state->path_start_x;
  float current_y = current_state->path_start_y;

  // Compute using de Casteljau's algorithm and output using quadratic_to_blob
  float qx1 = (current_x + 2 * control1_x) / 3;
  float qy1 = (current_y + 2 * control1_y) / 3;
  float qx2 = (to_x + 2 * control2_x) / 3;
  float qy2 = (to_y + 2 * control2_y) / 3;
  float rx = (current_x + 3 * control1_x + 3 * control2_x + to_x) / 8;
  float ry = (current_y + 3 * control1_y + 3 * control2_y + to_y) / 8;
  quadratic_to_blob(dfuncs, draw_data, current_state, qx1, qy1, rx, ry, nullptr);
  quadratic_to_blob(dfuncs, draw_data, current_state, qx2, qy2, to_x, to_y, nullptr);
}



static void
close_path_blob (hb_draw_funcs_t *dfuncs, user_data_t *draw_data, hb_draw_state_t *, void *)
{
  if (draw_data->consumed + 1 >= draw_data->size)
  {
      draw_data->failure = true;
      return;
  }

  draw_data->str[draw_data->consumed++] = 0;
}

int
hbjs_glyph_blob (hb_font_t *font, hb_codepoint_t glyph, char *buf, unsigned buf_size, unsigned units_per_em)
{
  if (blob_funcs == 0) /* not the best pattern for multi-threaded apps which is not a concern here */
  {
    blob_funcs = hb_draw_funcs_create (); /* will be leaked */
    hb_draw_funcs_set_move_to_func (blob_funcs, (hb_draw_move_to_func_t) move_to_blob, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func (blob_funcs, (hb_draw_line_to_func_t) line_to_blob, nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func (blob_funcs, (hb_draw_quadratic_to_func_t) quadratic_to_blob, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func (blob_funcs, (hb_draw_cubic_to_func_t) cubic_to_blob, nullptr, nullptr);
    hb_draw_funcs_set_close_path_func (blob_funcs, (hb_draw_close_path_func_t) close_path_blob, nullptr, nullptr);
  }

  user_data_t draw_data(buf, buf_size, units_per_em);
  buf[0] = 0;
  draw_data.consumed += 1;

  hb_font_get_glyph_shape (font, glyph, blob_funcs, &draw_data);
  if (draw_data.failure)
    return -1;

  return draw_data.consumed;
}

void
hbjs_set_weight (hb_font_t *font, unsigned weight) {
  hb_variation_t variations[1];
  variations[0].tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
  variations[0].value = weight;
  hb_font_set_variations (font, variations, 1);
}



static hb_bool_t do_trace (hb_buffer_t *buffer,
                           hb_font_t   *font,
                           const char  *message,
                           user_data_t *user_data) {
  unsigned int consumed;
  unsigned int num_glyphs = hb_buffer_get_length (buffer);

  if (strcmp(message, "start table GSUB") == 0) {
    user_data->current_phase = HB_SHAPE_GSUB_PHASE;
  } else if (strcmp(message, "start table GPOS") == 0) {
    user_data->current_phase = HB_SHAPE_GPOS_PHASE;
  }


  if (user_data->current_phase != user_data->stop_phase) {
    user_data->stopping = false;
  }

  // If we overflowed, keep going anyway.
  if (user_data->failure) return 1;

  if (user_data->stop_phase != HB_SHAPE_DONT_STOP) {
    // Do we need to start stopping?
    char buf[12];
    snprintf (buf, 12, "%d", user_data->stop_at);
    if ((user_data->current_phase == user_data->stop_phase) &&
        (strncmp(message, "end lookup ", 11) == 0) &&
        (strcmp(message + 11, buf) == 0)) {
      user_data->stopping = true;
    }
  }

  // If we need to stop, stop.
  if (user_data->stopping) return 0;

  _user_data_printf (user_data, "{\"m\":\"%s\",\"t\":", message);
  hb_buffer_serialize_glyphs(buffer, 0, num_glyphs,
    user_data->str + user_data->consumed,
    user_data->size - user_data->consumed,
    &consumed,
    font,
    HB_BUFFER_SERIALIZE_FORMAT_JSON,
    HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES);
  user_data->consumed += consumed;
  _user_data_printf (user_data, "},\n");

  return 1;
}

unsigned
hbjs_shape_with_trace (hb_font_t *font, hb_buffer_t* buf,
                       char* featurestring,
                       unsigned int stop_at, unsigned int stop_phase,
                       char *outbuf, unsigned buf_size) {
  user_data_t user_data(outbuf, buf_size, stop_at, stop_phase);

  int num_features = 0;
  hb_feature_t* features = nullptr;

  if (*featurestring) {
    /* count the features first, so we can allocate memory */
    char* p = featurestring;
    do {
      num_features++;
      p = strchr (p, ',');
      if (p)
        p++;
    } while (p);

    features = (hb_feature_t *) calloc (num_features, sizeof (*features));

    /* now do the actual parsing */
    p = featurestring;
    num_features = 0;
    while (p && *p) {
      char *end = strchr (p, ',');
      if (hb_feature_from_string (p, end ? end - p : -1, &features[num_features]))
        num_features++;
      p = end ? end + 1 : nullptr;
    }
  }

  hb_buffer_set_message_func (buf, (hb_buffer_message_func_t)do_trace, &user_data, nullptr);
  user_data.str[user_data.consumed++] = '[';
  hb_shape(font, buf, features, num_features);

  if (user_data.failure) return -1;

  user_data.str[user_data.consumed-2] = ']';
  user_data.str[user_data.consumed-1] = '\0';
  return user_data.consumed;
}

#ifdef MAIN
#include <stdio.h>
int main() {
    hb_blob_t *blob = hb_blob_create_from_file ("/home/ebrahim/Desktop/harfbuzzjs/harfbuzz/test/subset/data/fonts/Roboto-Regular.ttf");
    hb_face_t *face = hb_face_create (blob, 0);
    hb_blob_destroy (blob);
    hb_font_t *font = hb_font_create (face);
    hb_face_destroy (face);
    char buf[1024];
    buf[0] = '\0';
    printf ("%d %d\n", hb_blob_get_length (blob), hbjs_ot_glyph_svg (font, 0, buf, sizeof (buf)));
    puts (buf);
    hb_font_destroy (font);
}
#endif
