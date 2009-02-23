/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef RCFILE_H
#define RCFILE_H


void write_indent(GString *str, gint indent);
void write_char_option(GString *str, gint indent, const gchar *label, const gchar *text);
gboolean read_char_option(const gchar *option, const gchar *label, const gchar *value, gchar **text);
void write_color_option(GString *str, gint indent, gchar *label, GdkColor *color);
gboolean read_color_option(const gchar *option, const gchar *label, const gchar *value, GdkColor *color);
void write_int_option(GString *str, gint indent, const gchar *label, gint n);
gboolean read_int_option(const gchar *option, const gchar *label, const gchar *value, gint *n);
void write_uint_option(GString *str, gint indent, const gchar *label, guint n);
gboolean read_uint_option(const gchar *option, const gchar *label, const gchar *value, guint *n);
gboolean read_uint_option_clamp(const gchar *option, const gchar *label, const gchar *value, guint *n, guint min, guint max);
gboolean read_int_option_clamp(const gchar *option, const gchar *label, const gchar *value, gint *n, gint min, gint max);
void write_int_unit_option(GString *str, gint indent, gchar *label, gint n, gint subunits);
gboolean read_int_unit_option(const gchar *option, const gchar *label, const gchar *value, gint *n, gint subunits);
void write_bool_option(GString *str, gint indent, gchar *label, gint n);
gboolean read_bool_option(const gchar *option, const gchar *label, const gchar *value, gint *n);

#define WRITE_BOOL(source, _name_) write_bool_option(outstr, indent, #_name_, (source)._name_)
#define WRITE_INT(source, _name_) write_int_option(outstr, indent, #_name_, (source)._name_)
#define WRITE_UINT(source, _name_) write_uint_option(outstr, indent, #_name_, (source)._name_)
#define WRITE_INT_UNIT(source, _name_, _unit_) write_int_unit_option(outstr, indent, #_name_, (source)._name_, _unit_)
#define WRITE_CHAR(source, _name_) write_char_option(outstr, indent, #_name_, (source)._name_)
#define WRITE_COLOR(source, _name_) write_color_option(outstr, indent, #_name_, &(source)._name_)

#define WRITE_SEPARATOR() g_string_append(outstr, "\n")
#define WRITE_SUBTITLE(_title_) g_string_append_printf(outstr, "\n\n<!-- "_title_" -->\n\n")


#define READ_BOOL(target, _name_) if (read_bool_option(option, #_name_, value, &(target)._name_)) continue;
#define READ_INT(target, _name_) if (read_int_option(option, #_name_, value, &(target)._name_)) continue;
#define READ_UINT(target, _name_) if (read_uint_option(option, #_name_, value, &(target)._name_)) continue;
#define READ_INT_CLAMP(target, _name_, _min_, _max_) if (read_int_option_clamp(option, #_name_, value, &(target)._name_, _min_, _max_)) continue;
#define READ_UINT_CLAMP(target, _name_, _min_, _max_) if (read_uint_option_clamp(option, #_name_, value, &(target)._name_, _min_, _max_)) continue;
#define READ_INT_UNIT(target, _name_, _unit_) if (read_int_unit_option(option, #_name_, value, &(target)._name_, _unit_)) continue;
#define READ_CHAR(target, _name_) if (read_char_option(option, #_name_, value, &(target)._name_)) continue;
#define READ_COLOR(target, _name_) if (read_color_option(option, #_name_, value, &(target)._name_)) continue;

#define READ_BOOL_FULL(name, target) if (read_bool_option(option, name, value, &(target))) continue;
#define READ_INT_FULL(name, target) if (read_int_option(option, name, value, &(target))) continue;
#define READ_UINT_FULL(name, target) if (read_uint_option(option, name, value, &(target))) continue;
#define READ_INT_CLAMP_FULL(name, target, _min_, _max_) if (read_int_option_clamp(option, name, value, &(target), _min_, _max_)) continue;
#define READ_INT_UNIT_FULL(name, target, _unit_) if (read_int_unit_option(option, name, value, &(target), _unit_)) continue;
#define READ_CHAR_FULL(name, target) if (read_char_option(option, name, value, &(target))) continue;
#define READ_COLOR_FULL(name, target) if (read_color_option(option, name, value, &(target))) continue;




typedef struct _GQParserFuncData GQParserFuncData;
typedef struct _GQParserData GQParserData;
typedef	void (* GQParserStartFunc)(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error);
typedef	void (* GQParserEndFunc)(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, gpointer data, GError **error);

void options_parse_func_push(GQParserData *parser_data, GQParserStartFunc start_func, GQParserEndFunc end_func, gpointer data);
void options_parse_func_pop(GQParserData *parser_data);
void options_parse_func_set_data(GQParserData *parser_data, gpointer data);


gboolean save_options_to(const gchar *utf8_path, ConfOptions *options);
gboolean load_options_from(const gchar *utf8_path, ConfOptions *options, gboolean startup);


#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
