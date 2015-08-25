/*  File: zmapWindowStyle.c
 *  Author: Malcolm Hinsley (mh17@sanger.ac.uk)
 *  Copyright (c) 2006-2015: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originated by
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Code implementing the menus for sequence display.
 *
 * Exported functions: ZMap/zmapWindows.h
 *
 *-------------------------------------------------------------------
 */

/*
  called from zmapWindowMenu.c
  initially allows style fill and border colours to be set
  creating a child style if necessary
  then applies the new style to the featureset
  using the code in zmapWindowMenus.c

  new file started as a) windowMenus.c is quite long already and
  b) styles have a lot of other options we could set
*/


#include <ZMap/zmap.hpp>

#include <string.h>
#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapGLibUtils.hpp> /* zMap_g_hash_table_nth */
#include <zmapWindow_P.hpp>
#include <zmapWindowCanvasItem.hpp>
#include <zmapWindowContainerUtils.hpp>
#include <zmapWindowContainerFeatureSet_I.hpp>



typedef struct
{
  ItemMenuCBData menu_data;               /* which featureset etc */
  ZMapFeatureTypeStyleStruct orig_style_copy;  /* copy of original, used for Revert */

  gboolean refresh;                       /* clicked on another column */

  GtkWidget *toplevel ;
  GtkWidget *featureset_name_widget;      /* Name of the featureset whose style is being
                                           * edited/created */
  GtkWidget *style_name_widget;           /* User-editable style name for the new/existing style
                                           * to edit */

  GtkWidget *fill_widget ;                /* Fill colour button */ 
  GtkWidget *border_widget ;              /* Border colour button */ 
  GtkWidget *cds_fill_widget ;            /* CDS fill colour button */ 
  GtkWidget *cds_border_widget ;          /* CDS border colour button */
  GtkWidget *cds_container;               /* Container for the CDS colour buttons */ 

  GtkWidget *stranded;


} StyleChangeStruct, *StyleChange;


static void destroyCB(GtkWidget *widget, gpointer cb_data);
static void applyCB(GtkWidget *widget, gpointer cb_data);
static void okCB(GtkWidget *widget, gpointer cb_data);
static void revertCB(GtkWidget *widget, gpointer cb_data);
static void closeCB(GtkWidget *widget, gpointer cb_data);

static void updateStyle(StyleChange my_data, ZMapFeatureTypeStyle style);


/* Entry point to display the Edit Style dialog */
void zmapWindowShowStyleDialog( ItemMenuCBData menu_data )
{
  StyleChange my_data = g_new0(StyleChangeStruct,1);
  /* theoretically we could have more than one dialog active. How confusing that would be */
  ZMapFeatureTypeStyle style;
  const char *text = NULL;
  GtkWidget *toplevel, *top_vbox, *vbox, *hbox, *frame, *button, *label, *entry;
  GdkColor colour = {0} ;
  GdkColor *fill_col = &colour, *border_col = &colour;
  GQuark default_style_name = 0;

  /* Check if the dialog data has already been created - if so, nothing to do */
  if(zmapWindowStyleDialogSetFeature(menu_data->window, menu_data->item, menu_data->feature))
    return;

  my_data->menu_data = menu_data;
  style = menu_data->feature_set->style;

  /* set up the top level window */
  my_data->toplevel = toplevel = zMapGUIToplevelNew(NULL, "Edit Style") ;

  g_signal_connect(GTK_OBJECT(toplevel), "destroy", GTK_SIGNAL_FUNC(destroyCB), (gpointer)my_data) ;
  gtk_window_set_keep_above((GtkWindow *) toplevel,TRUE);
  gtk_container_set_focus_chain (GTK_CONTAINER(toplevel), NULL);

  gtk_container_border_width(GTK_CONTAINER(toplevel), 5) ;
  gtk_window_set_default_size(GTK_WINDOW(toplevel), 500, -1) ;

  /* Add ptr so parent knows about us */
  menu_data->window->style_window = (gpointer) my_data ;

  if (style->unique_id == menu_data->feature_set->unique_id)
    {
      /* The current style name is the same as the featureset name. This means that this
       * featureset probably "owns" this style, so by default we want to overwrite it */
      default_style_name = style->unique_id;
    }
  else
    {
      /* The current style name is different to the featureset name. This means that this
       * featureset probably DOESN'T own the style and therefore we want to create a new child
       * style named after the featureset name */
      default_style_name = menu_data->feature_set->original_id;
    }

  top_vbox = gtk_vbox_new(FALSE, 0) ;
  gtk_container_add(GTK_CONTAINER(toplevel), top_vbox) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(top_vbox), NULL);

  //      menubar = makeMenuBar(my_data) ;
  //      gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);


  text = "Style" ;
  frame = gtk_frame_new(text) ;
  gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.5);
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(top_vbox), frame, TRUE, TRUE, 0) ;

  vbox = gtk_vbox_new(FALSE, 0) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(vbox), NULL);
  gtk_container_add(GTK_CONTAINER(frame), vbox) ;

  /* The name of the featureset being edited (not editable) */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(hbox), NULL);
  gtk_container_add(GTK_CONTAINER(vbox), hbox) ;

  label = gtk_label_new("Featureset: ") ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;

  my_data->featureset_name_widget = entry = gtk_entry_new() ;
  gtk_entry_set_text(GTK_ENTRY(entry), g_quark_to_string(my_data->menu_data->feature_set->original_id)) ;
  gtk_widget_set_sensitive(entry, FALSE) ;
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0) ;

  /* The name of the style to edit (editable) */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(hbox), NULL);
  gtk_container_add(GTK_CONTAINER(vbox), hbox) ;

  label = gtk_label_new("Style name: ") ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;

  my_data->style_name_widget = entry = gtk_entry_new() ;
  gtk_entry_set_text(GTK_ENTRY(entry), g_quark_to_string(default_style_name)) ;
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0) ;


  /* Make colour buttons. */
  zMapStyleGetColours(style, STYLE_PROP_COLOURS, ZMAPSTYLE_COLOURTYPE_NORMAL, &fill_col, NULL, &border_col);


  frame = gtk_frame_new("Set Colours:") ;
  gtk_container_set_border_width(GTK_CONTAINER(frame),
                                 ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);
  gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, FALSE, 0) ;

  vbox = gtk_vbox_new(FALSE, 0) ;                /* vbox for rows of colours */
  gtk_container_add(GTK_CONTAINER(frame), vbox) ;

  hbox = gtk_hbox_new(FALSE, 0) ;                /* hbox for fill_col / border_col */
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0) ;
  gtk_box_set_spacing(GTK_BOX(hbox), ZMAP_WINDOW_GTK_BUTTON_BOX_SPACING) ;
  gtk_container_set_border_width(GTK_CONTAINER(hbox), ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);

  label = gtk_label_new("Fill:") ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  my_data->fill_widget = button = gtk_color_button_new() ;
  gtk_color_button_set_color(GTK_COLOR_BUTTON(button), fill_col) ;
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0) ;

  label = gtk_label_new("Border:") ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  my_data->border_widget = button = gtk_color_button_new() ;
  gtk_color_button_set_color(GTK_COLOR_BUTTON(button), border_col) ;
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0) ;

  if(style->mode == ZMAPSTYLE_MODE_TRANSCRIPT)        /* add CDS colours */
    {
      zMapStyleGetColours(style, STYLE_PROP_TRANSCRIPT_CDS_COLOURS, ZMAPSTYLE_COLOURTYPE_NORMAL, &fill_col, NULL, &border_col);
    }

  /* must create these anyway */
  {
    my_data->cds_container = hbox = gtk_hbox_new(FALSE, 0) ;                /* hbox for fill_col / border_col */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0) ;
    gtk_box_set_spacing(GTK_BOX(hbox), ZMAP_WINDOW_GTK_BUTTON_BOX_SPACING) ;
    gtk_container_set_border_width(GTK_CONTAINER(hbox), ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);

    label = gtk_label_new("CDS\nFill:") ;
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

    my_data->cds_fill_widget = button = gtk_color_button_new() ;
    gtk_color_button_set_color(GTK_COLOR_BUTTON(button), fill_col) ;
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0) ;

    label = gtk_label_new("CDS\nBorder:") ;
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

    my_data->cds_border_widget = button = gtk_color_button_new() ;
    gtk_color_button_set_color(GTK_COLOR_BUTTON(button), border_col) ;
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0) ;
  }

  if(style->mode == ZMAPSTYLE_MODE_TRANSCRIPT)        /* add CDS colours */
    {
      gtk_widget_hide_all(my_data->cds_container);
    }


  /* some major parameters */
  frame = gtk_frame_new("Layout:") ;
  gtk_container_set_border_width(GTK_CONTAINER(frame),
                                 ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);
  gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, FALSE, 0) ;

  vbox = gtk_vbox_new(FALSE, 0) ;                /* vbox for rows of colours */
  gtk_container_add(GTK_CONTAINER(frame), vbox) ;

  my_data->stranded = button = gtk_check_button_new_with_label("Stranded");
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  // if anything should be toggled
  //      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(strandedCB), my_data);

#if 0
  /* if alignment... */
  button = gtk_check_button_new_with_label("Always Gapped");
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(strandedCB), my_data);
#endif

  /* Make control buttons along bottom of dialog. */
  frame = gtk_frame_new(NULL) ;
  gtk_container_set_border_width(GTK_CONTAINER(frame),
                                 ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);
  gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, FALSE, 0) ;


  hbox = gtk_hbutton_box_new();
  gtk_container_add(GTK_CONTAINER(frame), hbox);
  gtk_box_set_spacing(GTK_BOX(hbox), ZMAP_WINDOW_GTK_BUTTON_BOX_SPACING);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);

  

  button = gtk_button_new_from_stock(GTK_STOCK_REVERT_TO_SAVED);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(revertCB), my_data);

  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(applyCB), my_data);

  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(closeCB), my_data);

  button = gtk_button_new_from_stock(GTK_STOCK_OK);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(okCB), my_data);

  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT) ; /* set ok button as default. */
  gtk_window_set_default(GTK_WINDOW(toplevel), button) ;

  gtk_widget_show_all(toplevel) ;

  zmapWindowStyleDialogSetFeature(menu_data->window, menu_data->item, menu_data->feature);

  return ;
}


/* Interface to allow external callers to destroy the style dialog */
void zmapWindowStyleDialogDestroy(ZMapWindow window)
{
  StyleChange my_data = (StyleChange) window->style_window;

  gtk_widget_destroy(my_data->toplevel);
  window->style_window = NULL;
}


/* This updates the info in the Edit Style dialog with the given feature, i.e. it takes all of the
 * style info from the given feature's style. It's called when the dialog is first opened or if
 * the user selects a different feature while the dialog is still open. */
gboolean zmapWindowStyleDialogSetFeature(ZMapWindow window, FooCanvasItem *foo, ZMapFeature feature)
{
  StyleChange my_data = (StyleChange) window->style_window;
  ZMapFeatureSet feature_set = (ZMapFeatureSet) feature->parent;
  ZMapFeatureTypeStyle style;
  GdkColor colour = {0} ;
  GdkColor *fill_col = &colour, *border_col = &colour;
  GQuark default_style_name ;

  if(!my_data)
    return FALSE;

  my_data->menu_data->item = foo;
  my_data->menu_data->feature_set = feature_set;
  my_data->menu_data->feature = feature;
  style = feature_set->style;

  /* Make a copy of the original style so we can revert any changes we make */
  memcpy(&my_data->orig_style_copy, style,sizeof (ZMapFeatureTypeStyleStruct));

  /* Update the featureset name */
  gtk_entry_set_text(GTK_ENTRY(my_data->featureset_name_widget), g_quark_to_string(feature_set->original_id)) ;

  /* Update the default style name */
  if (style->unique_id == feature_set->unique_id)
    {
      /* The current style name is the same as the featureset name. This means that this
       * featureset probably "owns" this style, so by default we want to overwrite it */
      default_style_name = style->unique_id;
    }
  else
    {
      /* The current style name is different to the featureset name. This means that this
       * featureset probably DOESN'T own the style and therefore we want to create a new child
       * style named after the featureset name */
      default_style_name = feature_set->original_id;
    }

  gtk_entry_set_text(GTK_ENTRY(my_data->style_name_widget), g_quark_to_string(default_style_name));
  
  /* Update the colour buttons. */
  zMapStyleGetColours(style, STYLE_PROP_COLOURS, ZMAPSTYLE_COLOURTYPE_NORMAL, &fill_col, NULL, &border_col);

  gtk_color_button_set_color(GTK_COLOR_BUTTON(my_data->fill_widget), fill_col) ;
  gtk_color_button_set_color(GTK_COLOR_BUTTON(my_data->border_widget), border_col) ;

  if(style->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      zMapStyleGetColours(style, STYLE_PROP_TRANSCRIPT_CDS_COLOURS, ZMAPSTYLE_COLOURTYPE_NORMAL, &fill_col, NULL, &border_col);

      gtk_color_button_set_color(GTK_COLOR_BUTTON(my_data->cds_fill_widget), fill_col) ;
      gtk_color_button_set_color(GTK_COLOR_BUTTON(my_data->cds_border_widget), border_col) ;
      gtk_widget_show_all(my_data->cds_container);
    }
  else
    {
      gtk_widget_hide_all(my_data->cds_container);
    }

  gtk_toggle_button_set_active((GtkToggleButton *) my_data->stranded, style->strand_specific);

  return TRUE;
}


/* Called when the user hits Revert on the Edit Style dialog. This throws away any changes since
 * they first opened the dialog, i.e. it reverts to the original style that was in the featureset. */
static void revertCB(GtkWidget *widget, gpointer cb_data)
{
  StyleChange my_data = (StyleChange) cb_data;
  ItemMenuCBData menu_data = my_data->menu_data;
  ZMapFeatureSet feature_set = menu_data->feature_set;
  ZMapFeatureTypeStyle style = NULL;
  GQuark id;

  /* If the current style is different to the original then we have created a child style. Just
     remove the child and replace it with the parent */
  if(feature_set->style->unique_id != my_data->orig_style_copy.unique_id)   
    {
      GHashTable *styles = my_data->menu_data->window->context_map->styles;
      style = feature_set->style;
      id = style->parent_id;

      /* free the created style: cannot be ref'd anywhere else */
      g_hash_table_remove(styles,GUINT_TO_POINTER(feature_set->style->unique_id));
      zMapStyleDestroy(style);

      /* find the parent */
      style = (ZMapFeatureTypeStyle)g_hash_table_lookup(styles, GUINT_TO_POINTER(id));
    }
  else                                /* restore the existing style */
    {
      /* Overwrite the current style from the copy of the original style */
      style = feature_set->style;
      memcpy((void *) style, (void *) &my_data->orig_style_copy, sizeof (ZMapFeatureTypeStyleStruct));
    }

  /* update the column */
  if (style)
    zmapWindowFeaturesetSetStyle(style->unique_id, feature_set, menu_data->context_map, menu_data->window);

  /* update this dialog */
  zmapWindowStyleDialogSetFeature(menu_data->window, menu_data->item, menu_data->feature);
}


/* Called when user hits OK on the Edit Style dialog. Just does an Apply then Close */
static void okCB(GtkWidget *widget, gpointer cb_data)
{
  applyCB(widget, cb_data) ;
  closeCB(widget, cb_data) ;
}


/* Called when the user applies changes on the Edit Style dialog. It updates the featureset's
 * style, creating a new child style if necessary  */
static void applyCB(GtkWidget *widget, gpointer cb_data)
{
  StyleChange my_data = (StyleChange) cb_data;
  ZMapFeatureSet feature_set = my_data->menu_data->feature_set;
  ZMapFeatureTypeStyle style = feature_set->style;
  GHashTable *styles = my_data->menu_data->window->context_map->styles;

  GQuark new_style_id = zMapStyleCreateID(gtk_entry_get_text(GTK_ENTRY(my_data->style_name_widget))) ;

  /* We make a new child style if the new style name is different to the existing one */
  if (new_style_id != style->unique_id)
    {
      /* make new style with the specified name and merge in the parent */
      ZMapFeatureTypeStyle parent = style;
      ZMapFeatureTypeStyle tmp_style;

      const char *name = g_quark_to_string(new_style_id) ;
      style = zMapStyleCreate(name, name);

      /* merge is written upside down
       * we have to copy the parent and merge the child onto it
       * then delete the style we just created
       */

      tmp_style = zMapFeatureStyleCopy(parent) ;

      if (zMapStyleMerge(tmp_style, style))
        {
          g_hash_table_insert(styles,GUINT_TO_POINTER(style->unique_id),tmp_style);
          zMapStyleDestroy(style);
          style = tmp_style;

          g_object_set(G_OBJECT(style), ZMAPSTYLE_PROPERTY_PARENT_STYLE, parent->unique_id , NULL);
        }
      else
        {
          zMapWarning("Cannot create new style %s",name);
          zMapStyleDestroy(tmp_style);
          zMapStyleDestroy(style);
          return;
        }
    }

  /* apply the chosen colours etc */
  updateStyle(my_data, style);

  /* set the style
   * if we are only changing colours we could be more efficient
   * but this will work for all cases and means no need to write more code
   * we do this _once_ at user/ mouse click speed
   */
  zmapWindowFeaturesetSetStyle(style->unique_id, feature_set, my_data->menu_data->context_map, my_data->menu_data->window);
}


/* Called when the user closes the Edit Style dialog. Just destroys the dialog without doing anything. */
static void closeCB(GtkWidget *widget, gpointer cb_data)
{
  StyleChange my_data = (StyleChange) cb_data ;

  gtk_widget_destroy(my_data->toplevel);
}


/* Destroy the data associated with the Edit Style dialog */
static void destroyCB(GtkWidget *widget, gpointer cb_data)
{
  StyleChange my_data = (StyleChange) cb_data;

  my_data->menu_data->window->style_window = NULL;

  g_free(my_data->menu_data);
  g_free(my_data);
}


/* Get the color spec string for the given color buttons. The result should be free'd by the caller
 * with g_free. Returns null if there was a problem. */
static char* getColorSpecStr(GtkWidget *fill_widget, GtkWidget *border_widget)
{
  char *colour_spec = NULL ;

  if (fill_widget && border_widget && 
      GTK_IS_COLOR_BUTTON(fill_widget) && GTK_IS_COLOR_BUTTON(border_widget))
    {
      GdkColor colour ;

      gtk_color_button_get_color(GTK_COLOR_BUTTON(fill_widget), &colour) ;
      char *fill_str = gdk_color_to_string(&colour) ;

      gtk_color_button_get_color(GTK_COLOR_BUTTON(border_widget), &colour) ;
      char *border_str = gdk_color_to_string(&colour) ;

      colour_spec = zMapStyleMakeColourString(fill_str, "black", border_str,
                                              fill_str, "black", border_str) ;
    }

  return colour_spec ;
}


/* Set the properties in the given style from the user-selected options from the dialog */
static void updateStyle(StyleChange my_data, ZMapFeatureTypeStyle style)
{
  /* Set whether strand-specific */
  gboolean stranded = gtk_toggle_button_get_active((GtkToggleButton *) my_data->stranded) ;
  g_object_set(G_OBJECT(style), ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC, stranded, NULL);


  /* Set the colours */
  char *colour_spec = getColorSpecStr(my_data->fill_widget, my_data->border_widget) ;

  if (colour_spec)
    {
      g_object_set(G_OBJECT(style), ZMAPSTYLE_PROPERTY_COLOURS, colour_spec, NULL);
      g_free(colour_spec) ;
      colour_spec = NULL ;
    }


  /* Set the CDS colours, if it's a transcript */
  if(style->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      colour_spec = getColorSpecStr(my_data->cds_fill_widget, my_data->cds_border_widget) ;

      if (colour_spec)
        {
          g_object_set(G_OBJECT(style), ZMAPSTYLE_PROPERTY_TRANSCRIPT_CDS_COLOURS, colour_spec, NULL);
          g_free(colour_spec) ;
          colour_spec = NULL ;
        }
    }

}


/* Sets the style in the given feature_set to the style given by style_id  */
void zmapWindowFeaturesetSetStyle(GQuark style_id, 
                                  ZMapFeatureSet feature_set,
                                  ZMapFeatureContextMap context_map,
                                  ZMapWindow window)
{
  ZMapFeatureTypeStyle style;
  FooCanvasItem *set_item, *canvas_item;
  int set_strand, set_frame;
  ID2Canvas id2c;
  int ok = FALSE;

  style = (ZMapFeatureTypeStyle)g_hash_table_lookup( context_map->styles, GUINT_TO_POINTER(style_id));

  if(style)
    {
      ZMapFeatureColumn column;
      ZMapFeatureSource s2s;
      ZMapFeatureSetDesc f2c;
      GList *c2s;

      /* now tweak featureset & column styles in various places */
      s2s = (ZMapFeatureSource)g_hash_table_lookup(context_map->source_2_sourcedata,GUINT_TO_POINTER(feature_set->unique_id));
      f2c = (ZMapFeatureSetDesc)g_hash_table_lookup(context_map->featureset_2_column,GUINT_TO_POINTER(feature_set->unique_id));
      if(s2s && f2c)
        {
          s2s->style_id = style->unique_id;
          column = (ZMapFeatureColumn)g_hash_table_lookup(context_map->columns,GUINT_TO_POINTER(f2c->column_id));
          if(column)
            {

              c2s = (GList *)g_hash_table_lookup(context_map->column_2_styles,GUINT_TO_POINTER(f2c->column_id));
              if(c2s)
                {
                  g_list_free(column->style_table);
                  column->style_table = NULL;
                  column->style = NULL;        /* must clear this to trigger style table calculation */
                  /* NOTE column->style_id is column specific not related to a featureset, set by config */

                  for( ; c2s; c2s = c2s->next)
                    {
                      if(GPOINTER_TO_UINT(c2s->data) == feature_set->style->unique_id)
                        {
                          c2s->data = GUINT_TO_POINTER(style->unique_id);
                          break;
                        }
                    }
                  ok = TRUE;
                }

              zMapWindowGetSetColumnStyle(window, feature_set->unique_id);
            }
        }
    }

  if(!ok)
    {
      zMapWarning("cannot set new style","");
      return;
    }


  /* get current style strand and frame status and operate on 1 or more columns
   * NOTE that the FToIhash has diff hash tables per strand and frame
   * for each one remove the FtoIhash and remove the set from the column
   * if the column is empty the destroy it
   * actaull it's easier just to cycle round all the possible strand and frame combos
   * 3 hash table lookups for each, 8x
   *
   * then set the new style and redisplay the featureset
   * strand and frame will be handled by the display code
   */

  /* yes really: reverse is bigger than forwards despite appearing on the left */
  for(set_strand = ZMAPSTRAND_NONE; set_strand <= ZMAPSTRAND_REVERSE; set_strand++)
    {
      /* yes really: frames are numbered 0,1,2 and have the values 1,2,3 */
      for(set_frame = ZMAPFRAME_NONE; set_frame <= ZMAPFRAME_2; set_frame++)
        {
          ZMapStrand strand = (ZMapStrand)set_strand ;
          ZMapFrame frame = (ZMapFrame)set_frame ;

          /* this is really frustrating:
           * every operation of the ftoi hash involves
           * repeating the same nested hash table lookups
           */



          /* does the set appear in a column ? */
          /* set item is a ContainerFeatureset */
          set_item = zmapWindowFToIFindSetItem(window,
                                               window->context_to_item,
                                               feature_set, strand, frame);
          if(!set_item)
            continue;

          /* find the canvas item (CanvasFeatureset) containing a feature in this set */
          id2c = zmapWindowFToIFindID2CFull(window, window->context_to_item,
                                            feature_set->parent->parent->unique_id,
                                            feature_set->parent->unique_id,
                                            feature_set->unique_id,
                                            strand, frame,0);

          canvas_item = NULL;
          if(id2c)
            {
              ID2Canvas feat = (ID2Canvas)zMap_g_hash_table_nth(id2c->hash_table,0);
              if(feat)
                canvas_item = feat->item;
            }


          /* look it up again to delete it :-( */
          zmapWindowFToIRemoveSet(window->context_to_item,
                                  feature_set->parent->parent->unique_id,
                                  feature_set->parent->unique_id,
                                  feature_set->unique_id,
                                  strand, frame, TRUE);


          if(canvas_item)
            {
              FooCanvasGroup *group = (FooCanvasGroup *) set_item;

              /* remove this featureset from the CanvasFeatureset */
              /* if it's empty it will perform hari kiri */
              if(ZMAP_IS_WINDOW_FEATURESET_ITEM(canvas_item))
                {
                  zMapWindowFeaturesetItemRemoveSet(canvas_item, feature_set, TRUE);
                }

              /* destroy set item if empty ? */
              if(!group->item_list)
                zmapWindowContainerGroupDestroy((ZMapWindowContainerGroup) set_item);
            }

          zmapWindowRemoveIfEmptyCol((FooCanvasGroup **) &set_item) ;

        }
    }


  feature_set->style = style;

  zmapWindowRedrawFeatureSet(window, feature_set);        /* does a complex context thing */

  zmapWindowColOrderColumns(window) ;        /* put this column (deleted then created) back into the right place */
  zmapWindowFullReposition(window->feature_root_group,TRUE, "window style") ;                /* adjust sizing and shuffle left / right */

  return ;
}

