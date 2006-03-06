#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gtk/gtktable.h>
#include <libgnomevfs/gnome-vfs.h>
#include "libgviewer.h"

#define DEFAULT_TAB_SIZE 8
#define DEFAULT_WRAP_MODE TRUE
#define DEFAULT_FIXED_LIMIT 80
#define DEFAULT_ENCODING "ASCII"
#define DEFAULT_BEST_FIT TRUE
#define DEFAULT_SCALE_FACTOR 1.0


/* Class Private Data */
struct _GViewerPrivate
{
	GtkWidget *tscrollbox;
	TextRender *textr ;

	GtkWidget *iscrollbox;
	ImageRender *imgr ;
	gboolean    img_initialized ;
	
	GtkWidget *last_client ;
	
	gchar 	   *filename ;
	VIEWERDISPLAYMODE dispmode;

};

static GtkTableClass *parent_class = NULL ;
enum {
  STATUS_LINE_CHANGED,
  LAST_SIGNAL
};

static guint gviewer_signals[LAST_SIGNAL] = { 0 };

/* Gtk class related static functions */
static void gviewer_init (GViewer *w);
static void gviewer_class_init (GViewerClass *class);
static void gviewer_destroy (GtkObject *object);
	
static void gviewer_text_status_update(TextRender *obj, TextRenderStatus *status, GViewer *viewer);
static void gviewer_image_status_update(ImageRender *obj, ImageRenderStatus *status, GViewer *viewer);

static VIEWERDISPLAYMODE guess_display_mode(const unsigned char* data, int len);
static void gviewer_auto_detect_display_mode(GViewer *obj);
/*****************************************
	public functions
	(defined in the header file)
*****************************************/
GtkType gviewer_get_type (void)
{
	static GtkType type = 0;
	if (type == 0)	{
		GTypeInfo info =
		{
			sizeof (GViewerClass),
			NULL,
			NULL,
			(GClassInitFunc) gviewer_class_init,
			NULL,
			NULL,
			sizeof(GViewer),
			0,
			(GInstanceInitFunc) gviewer_init
		};
		type = g_type_register_static(GTK_TYPE_TABLE, "gviewerwidget", &info,0);
	}
	
	return type;
}

GtkWidget*
gviewer_new (void)
{
	GViewer *w;
	
	w = gtk_type_new (gviewer_get_type ());
	
	return GTK_WIDGET (w);
}

static void
gviewer_class_init (GViewerClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS(class);
	
	parent_class = gtk_type_class (gtk_table_get_type ());
	
	object_class->destroy = gviewer_destroy;

	gviewer_signals[STATUS_LINE_CHANGED] =
        gtk_signal_new ("status_line_changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GViewerClass, status_line_changed),
            gtk_marshal_NONE__STRING,
            GTK_TYPE_NONE,
            1, GTK_TYPE_STRING);
}

static void
gviewer_init (GViewer *w)
{
	w->priv = g_new0(GViewerPrivate,1);
	
	gtk_table_resize(GTK_TABLE(w), 1,1);	
	gtk_table_set_homogeneous(GTK_TABLE(w),FALSE);

	w->priv->img_initialized = FALSE ;
	w->priv->dispmode = DISP_MODE_TEXT_FIXED ;
	
	w->priv->textr = (TextRender*) text_render_new() ;
	
	gviewer_set_tab_size(w, DEFAULT_TAB_SIZE);
	gviewer_set_wrap_mode(w, DEFAULT_WRAP_MODE);
	gviewer_set_fixed_limit(w, DEFAULT_FIXED_LIMIT);
	gviewer_set_encoding(w, DEFAULT_ENCODING);
	
	w->priv->tscrollbox = scroll_box_new() ;
	text_render_set_v_adjustment(w->priv->textr,
		scroll_box_get_v_adjustment(SCROLL_BOX(w->priv->tscrollbox))) ;
	text_render_set_h_adjustment(w->priv->textr,
		scroll_box_get_h_adjustment(SCROLL_BOX(w->priv->tscrollbox))) ;
	text_render_attach_external_v_range(w->priv->textr,
		scroll_box_get_v_range(SCROLL_BOX(w->priv->tscrollbox))) ;
	scroll_box_set_client(SCROLL_BOX(w->priv->tscrollbox),GTK_WIDGET(w->priv->textr)) ;
	gtk_widget_show(GTK_WIDGET(w->priv->textr));
	gtk_widget_show(w->priv->tscrollbox);
	g_object_ref(G_OBJECT(w->priv->tscrollbox)) ;
	
	w->priv->imgr  = (ImageRender*) image_render_new() ;
	gviewer_set_best_fit(w, DEFAULT_BEST_FIT);
	gviewer_set_scale_factor(w, DEFAULT_SCALE_FACTOR);
	w->priv->iscrollbox = scroll_box_new() ;
	image_render_set_v_adjustment(w->priv->imgr,
		scroll_box_get_v_adjustment(SCROLL_BOX(w->priv->iscrollbox))) ;
	image_render_set_h_adjustment(w->priv->imgr,
		scroll_box_get_h_adjustment(SCROLL_BOX(w->priv->iscrollbox))) ;
	image_render_set_best_fit(w->priv->imgr, TRUE);
	image_render_set_scale_factor(w->priv->imgr, 1);
	scroll_box_set_client(SCROLL_BOX(w->priv->iscrollbox),GTK_WIDGET(w->priv->imgr)) ;
	gtk_widget_show(GTK_WIDGET(w->priv->imgr));
	gtk_widget_show(w->priv->iscrollbox);
	g_object_ref(G_OBJECT(w->priv->iscrollbox)) ;

	w->priv->last_client = w->priv->tscrollbox;
	gtk_table_attach (GTK_TABLE(w), GTK_WIDGET(w->priv->tscrollbox), 0, 1, 0, 1,
		(GtkAttachOptions)(GTK_FILL|GTK_EXPAND),
		(GtkAttachOptions)(GTK_FILL|GTK_EXPAND), 0, 0);
	
	g_signal_connect(G_OBJECT(w),"destroy-event", G_CALLBACK(gviewer_destroy), (gpointer)w) ;	

	g_signal_connect(G_OBJECT(w->priv->textr),"text_status_changed", G_CALLBACK(gviewer_text_status_update), (gpointer)w) ;
	g_signal_connect(G_OBJECT(w->priv->imgr),"image_status_changed", G_CALLBACK(gviewer_image_status_update), (gpointer)w) ;
}

#define MAX_STATUS_LENGTH 100
static void gviewer_text_status_update(TextRender *obj, TextRenderStatus *status, GViewer *viewer)
{
	gchar temp[MAX_STATUS_LENGTH];
	
	g_return_if_fail (viewer!= NULL);
	g_return_if_fail (IS_GVIEWER (viewer));
	g_return_if_fail (status!=NULL);
	
	g_snprintf(temp,MAX_STATUS_LENGTH,
		"Position: %lu of %lu\tColumn: %d\t%s",
		(unsigned long)status->current_offset,
		(unsigned long)status->size,
		status->column,
		status->wrap_mode?"Wrap":"");
	
	gtk_signal_emit (
		GTK_OBJECT(viewer), gviewer_signals[STATUS_LINE_CHANGED], temp);
}

static void gviewer_image_status_update(ImageRender *obj, ImageRenderStatus *status, GViewer *viewer)
{
	gchar temp[MAX_STATUS_LENGTH];
	gchar scale[10];
	
	g_return_if_fail (viewer!= NULL);
	g_return_if_fail (IS_GVIEWER (viewer));
	g_return_if_fail (status!=NULL);
	
	if (!status->best_fit){
		g_snprintf(scale,10,"%d %%",
			(int)(status->scale_factor*100.0));
	}
	g_snprintf(temp,MAX_STATUS_LENGTH,
		"Image Size: %dx%d\tBits/Sample: %d\tZoom: %s",
		status->image_width,status->image_height,status->bits_per_sample,
		status->best_fit?"Fit-to-Window":scale);
	
	gtk_signal_emit (
		GTK_OBJECT(viewer), gviewer_signals[STATUS_LINE_CHANGED], temp);
}

static void
gviewer_destroy (GtkObject *widget)
{
	GViewer *w;
	
	g_return_if_fail (widget!= NULL);
	g_return_if_fail (IS_GVIEWER (widget));
	
	w = GVIEWER (widget);
	
	if (w->priv) {
		g_object_unref(G_OBJECT(w->priv->iscrollbox)) ;
		g_object_unref(G_OBJECT(w->priv->tscrollbox)) ;
		
		g_free(w->priv);
		w->priv = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(widget);
}

static VIEWERDISPLAYMODE guess_display_mode(const unsigned char* data, int len)
{
	int i ;
	gboolean control_chars = FALSE ; /* True if found ASCII < 0x20 */
	gboolean ascii_chars = FALSE ;
	gboolean extended_chars = FALSE; /* True if found ASCII >= 0x80 */
	const char* mime=NULL;

	mime = gnome_vfs_get_mime_type_for_data(data,len) ;

	if (g_strncasecmp(mime,"image/",6)==0)
		return DISP_MODE_IMAGE;

	
	/* Hex File ?  */
	for (i=0;i<len;i++) {
		if (data[i]<0x20 && data[i]!=10 && data[i]!=13 && data[i]!=9)
			control_chars = TRUE ;
		if (data[i]>=0x80)
			extended_chars = TRUE ;
		if (data[i]>=0x20 && data[i]<=0x7F)
			ascii_chars = TRUE ;
		/* TODO: add UTF-8 detection */
	}

	if (control_chars)
		return DISP_MODE_BINARY ;

	return DISP_MODE_TEXT_FIXED ;
}

void gviewer_auto_detect_display_mode(GViewer *obj)
{
	#define DETECTION_BUF_LEN 100
	int i ;
	int count ;
	unsigned char temp[DETECTION_BUF_LEN] ;
	ViewerFileOps* fops = NULL ;
	
	g_return_if_fail(obj!=NULL);
	obj->priv->dispmode = DISP_MODE_TEXT_FIXED ;

	if (obj->priv->textr==NULL)
		return;

	fops = text_render_get_file_ops(obj->priv->textr);
	if (fops==NULL)
		return;
	
	count = MIN(DETECTION_BUF_LEN,
		gv_file_get_max_offset(fops)) ;

	for (i=0;i<count;i++)
		temp[i] = gv_file_get_byte(fops,i) ;
	
	obj->priv->dispmode = guess_display_mode(temp,count);
}

void gviewer_set_display_mode(GViewer *obj, VIEWERDISPLAYMODE mode)
{
	GtkWidget *client = NULL;
	
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	
	if (mode==DISP_MODE_IMAGE && !obj->priv->img_initialized) {
		/* do lazy-initialization of the image render, only when the user
		    first asks to display the file as image */
		obj->priv->img_initialized = TRUE ;
		image_render_load_file(obj->priv->imgr, obj->priv->filename);
	}
	
	obj->priv->dispmode = mode ;
	switch (mode)
	{
	case DISP_MODE_TEXT_FIXED:
		client = obj->priv->tscrollbox;
		text_render_set_display_mode(obj->priv->textr, TR_DISP_MODE_TEXT);
		break;
	
	case DISP_MODE_BINARY:
		client = obj->priv->tscrollbox;
		text_render_set_display_mode(obj->priv->textr,
			TR_DISP_MODE_BINARY);
		break;
	
	case DISP_MODE_HEXDUMP:
		client = obj->priv->tscrollbox;
		text_render_set_display_mode(obj->priv->textr,
			TR_DISP_MODE_HEXDUMP);
		break;
	
	case DISP_MODE_IMAGE:
		client = obj->priv->iscrollbox;
		break;
	}
	
	if (client != obj->priv->last_client) {
		if (obj->priv->last_client!=NULL) {
			gtk_container_remove(GTK_CONTAINER(obj), obj->priv->last_client);
		}
		
		gtk_widget_grab_focus(GTK_WIDGET(client));
		gtk_table_attach (GTK_TABLE (obj), client , 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);	

		gtk_widget_show(client);
		obj->priv->last_client = client ;
	}
}

VIEWERDISPLAYMODE gviewer_get_display_mode(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL, DISP_MODE_TEXT_FIXED);
	g_return_val_if_fail (IS_GVIEWER (obj), DISP_MODE_TEXT_FIXED);
	
	return obj->priv->dispmode;
}

void gviewer_load_filedesc(GViewer *obj, int fd)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (fd>2);
	
	if (obj->priv->filename)
		g_free(obj->priv->filename);
	obj->priv->filename = NULL ;
	
	text_render_load_filedesc(obj->priv->textr, fd) ;

	gviewer_auto_detect_display_mode(obj);
	
	gviewer_set_display_mode(obj, obj->priv->dispmode);
}

void gviewer_load_file(GViewer *obj, const gchar*filename)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (filename);
	
	if (obj->priv->filename)
		g_free(obj->priv->filename);
	obj->priv->filename = NULL ;

	obj->priv->filename = g_strdup(filename);
	
	text_render_load_file(obj->priv->textr, obj->priv->filename) ;

	gviewer_auto_detect_display_mode(obj);
	
	gviewer_set_display_mode(obj, obj->priv->dispmode);
}

const gchar* gviewer_get_filename(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL, NULL);
	g_return_val_if_fail (IS_GVIEWER (obj), NULL);
	g_return_val_if_fail (obj->priv->filename, NULL);
	
	return obj->priv->filename;
}

void gviewer_set_tab_size(GViewer *obj, int tab_size)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_tab_size(obj->priv->textr,tab_size);
}

int gviewer_get_tab_size(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,0);
	g_return_val_if_fail (IS_GVIEWER (obj),0);
	g_return_val_if_fail (obj->priv->textr,0);
	
	return text_render_get_tab_size(obj->priv->textr);
}

void gviewer_set_wrap_mode(GViewer *obj, gboolean ACTIVE)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_wrap_mode(obj->priv->textr, ACTIVE);
}

gboolean gviewer_get_wrap_mode(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,FALSE);
	g_return_val_if_fail (IS_GVIEWER (obj),FALSE);
	g_return_val_if_fail (obj->priv->textr,FALSE);
	
	return text_render_get_wrap_mode(obj->priv->textr);
}


void gviewer_set_fixed_limit(GViewer *obj, int fixed_limit)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_fixed_limit(obj->priv->textr,fixed_limit);
}

int gviewer_get_fixed_limit(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,0);
	g_return_val_if_fail (IS_GVIEWER (obj),0);
	g_return_val_if_fail (obj->priv->textr,0);
	
	return text_render_get_fixed_limit(obj->priv->textr);
}

void gviewer_set_encoding(GViewer *obj, const char* encoding)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_encoding(obj->priv->textr,encoding);
}

const gchar* gviewer_get_encoding(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,NULL);
	g_return_val_if_fail (IS_GVIEWER (obj),NULL);
	g_return_val_if_fail (obj->priv->textr,NULL);
	
	return text_render_get_encoding(obj->priv->textr);
}

void gviewer_set_font_size(GViewer *obj, int font_size)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_font_size(obj->priv->textr,font_size);
}

int gviewer_get_font_size(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,0);
	g_return_val_if_fail (IS_GVIEWER (obj),0);
	g_return_val_if_fail (obj->priv->textr,0);
	
	return text_render_get_font_size(obj->priv->textr);
}


void gviewer_set_hex_offset_display(GViewer *obj, gboolean HEX_OFFSET)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	text_render_set_hex_offset_display(obj->priv->textr,HEX_OFFSET);
}

gboolean gviewer_get_hex_offset_display(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,FALSE);
	g_return_val_if_fail (IS_GVIEWER (obj),FALSE);
	g_return_val_if_fail (obj->priv->textr,FALSE);
	
	return text_render_get_hex_offset_display(obj->priv->textr);
}

void gviewer_set_best_fit(GViewer *obj, gboolean active)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->imgr);
	
	image_render_set_best_fit(obj->priv->imgr,active);
}

gboolean gviewer_get_best_fit(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,FALSE);
	g_return_val_if_fail (IS_GVIEWER (obj),FALSE);
	g_return_val_if_fail (obj->priv->textr,FALSE);
	
	return image_render_get_best_fit(obj->priv->imgr);
}


void gviewer_set_scale_factor(GViewer *obj, double scalefactor)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->imgr);
	
	image_render_set_scale_factor(obj->priv->imgr, scalefactor);
}

double gviewer_get_scale_factor(GViewer *obj)
{
	g_return_val_if_fail (obj!= NULL,0);
	g_return_val_if_fail (IS_GVIEWER (obj),0);
	g_return_val_if_fail (obj->priv->textr,0);
	
	return image_render_get_scale_factor(obj->priv->imgr);
}

void  gviewer_image_operation(GViewer *obj, IMAGEOPERATION op)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->imgr);
	
	return image_render_operation(obj->priv->imgr, op);
}

void gviewer_copy_selection(GViewer *obj)
{
	g_return_if_fail (obj!= NULL);
	g_return_if_fail (IS_GVIEWER (obj));
	g_return_if_fail (obj->priv->textr);
	
	if (obj->priv->dispmode!=DISP_MODE_IMAGE)
		text_render_copy_selection(obj->priv->textr);
}
