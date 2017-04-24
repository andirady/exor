#include "config.h"

#define PALETTE_SIZE 16

GdkRGBA bg;
GdkRGBA fg;
GdkRGBA palette[PALETTE_SIZE];
PangoFontDescription *font_desc;

enum SplitOrientation {
    HORIZONTAL,
    VERTICAL
};

static void on_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void on_child_exited(VteTerminal *term, gint status, gpointer data)
{
    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(term));
    gtk_container_remove(GTK_CONTAINER(parent), GTK_WIDGET(term));

    GList *list = gtk_container_get_children(GTK_CONTAINER(parent));
    while (list != NULL)
    {
        GtkWidget *child = GTK_WIDGET(list->data);
        g_object_ref(child);
        gtk_container_remove(GTK_CONTAINER(parent), child);
        GtkWidget *new_parent = gtk_widget_get_parent(parent);
        gtk_container_remove(GTK_CONTAINER(new_parent), parent);
        if (GTK_IS_PANED(new_parent))
        {
            if (gtk_paned_get_child1(GTK_PANED(new_parent)) == NULL)
                gtk_paned_add1(GTK_PANED(new_parent), child);
            else
                gtk_paned_add2(GTK_PANED(new_parent), child);
        }
        else if (GTK_IS_WINDOW(new_parent))
        {
            gtk_container_add(GTK_CONTAINER(new_parent), child);
        }
        g_object_unref(child);
        g_object_set(child, "has-focus", TRUE, NULL);
        break;
    }

    // Exit if there's no more terminals.
    if (list == NULL) gtk_main_quit();
}

static void on_increase_or_decrease_font_size(VteTerminal *term, gpointer data)
{
    gdouble font_scale = vte_terminal_get_font_scale(term);

    if (data != NULL) {
        vte_terminal_set_font_scale(term, font_scale + 0.5);
    } else {
        vte_terminal_set_font_scale(term, font_scale - 0.5);
    }
}

static void on_terminal_window_title_changed(VteTerminal *term, gpointer data)
{
    gtk_window_set_title(GTK_WINDOW(data), vte_terminal_get_window_title(term));
}

static void on_current_directory_uri_changed(VteTerminal *term, gpointer data)
{
    g_print(vte_terminal_get_current_directory_uri(term));
}

static GtkWidget* new_term(GtkWidget *main_window, GError **error)
{
    char *argv[] = {"/usr/bin/bash", NULL};
    GPid pid;

    GtkWidget *vte = vte_terminal_new();
    vte_terminal_set_color_foreground(VTE_TERMINAL(vte), &fg);
    vte_terminal_set_color_background(VTE_TERMINAL(vte), &bg);
    //vte_terminal_set_colors(VTE_TERMINAL(vte), &fg, &bg, palette, PALETTE_SIZE);
    vte_terminal_spawn_sync(VTE_TERMINAL(vte),
                            VTE_PTY_DEFAULT,
                            getenv("HOME"),
                            argv,
                            NULL,
                            G_SPAWN_DEFAULT,
                            NULL,
                            NULL,
                            &pid,
                            NULL,
                            error);

    gboolean b = TRUE;
    g_signal_connect(vte,
                     "increase-font-size",
                     G_CALLBACK(on_increase_or_decrease_font_size),
                     &b);
    g_signal_connect(vte,
                     "decrease-font-size",
                     G_CALLBACK(on_increase_or_decrease_font_size),
                     NULL);
    g_signal_connect(vte,
                     "child-exited",
                     G_CALLBACK(on_child_exited),
                     NULL);
    g_signal_connect(vte,
                     "window-title-changed",
                     G_CALLBACK(on_terminal_window_title_changed),
                     main_window);
    /*
    g_signal_connect(vte,
                     "current-directory-uri-changed",
                     G_CALLBACK(on_current_directory_uri_changed),
                     NULL);
    */
    return vte;
}

GtkCssProvider *css = NULL;

void modify_css(GtkWidget *widget) {
    GError *error = NULL;
    if (css == NULL) {
        css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,
                                        "paned separator {"
                                        "  background: #656560; "
                                        "}\0",
                                        -1,
                                        &error);
        if (error != NULL) {
            fprintf(stderr, "Error: %s\n", error->message);
            g_error_free(error);
            return;
        }
    }
    GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(ctx,
                                   GTK_STYLE_PROVIDER(css),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void split(GtkWidget *main_window, GtkOrientation orientation)
{
    GtkWidget *focus = gtk_container_get_focus_child(GTK_CONTAINER(main_window));
    if (focus == NULL) {
        fprintf(stderr, "Error: No focus child.\n");
        return;
    }
    if (GTK_IS_PANED(focus)) {
        while (!VTE_IS_TERMINAL(focus))
            focus = gtk_container_get_focus_child(GTK_CONTAINER(focus));
    }
    if (VTE_IS_TERMINAL(focus)) {
        GtkWidget *vte;
        GtkAllocation alloc;
        gtk_widget_get_allocation(focus, &alloc);

        GtkWidget *parent = gtk_widget_get_parent(focus);
        GtkWidget *paned = gtk_paned_new(orientation);

        g_object_ref(focus);
        gtk_container_remove(GTK_CONTAINER(parent), focus);
        gtk_container_add(GTK_CONTAINER(parent), paned);
        gtk_paned_add1(GTK_PANED(paned), focus);
        g_object_unref(focus);

        vte = new_term(main_window, NULL);
        gtk_paned_add2(GTK_PANED(paned), vte);
        gtk_widget_show_all(paned);

        if (orientation == GTK_ORIENTATION_HORIZONTAL) {
            modify_css(paned);
            gtk_paned_set_position(GTK_PANED(paned), alloc.width / 2);
        } else {
            modify_css(paned);
            gtk_paned_set_position(GTK_PANED(paned), alloc.height / 2);
        }

        g_object_set(vte, "has-focus", TRUE, NULL);
    }
}

static gboolean split_horizontal(GtkAccelGroup *sender, gpointer data)
{
    split(GTK_WIDGET(data), GTK_ORIENTATION_HORIZONTAL);

    return TRUE;
}

static gboolean split_vertical(GtkAccelGroup *sender, gpointer data)
{
    split(GTK_WIDGET(data), GTK_ORIENTATION_VERTICAL);

    return TRUE;
}

static gboolean copy_clipboard(GtkAccelGroup *sender, gpointer data)
{
    GtkWidget *focus = gtk_container_get_focus_child(GTK_CONTAINER(data));
    while (!VTE_IS_TERMINAL(focus)) {
        focus = gtk_container_get_focus_child(GTK_CONTAINER(focus));
    }
    vte_terminal_copy_clipboard(VTE_TERMINAL(focus));

    return TRUE;
}

static gboolean paste_clipboard(GtkAccelGroup *sender, gpointer data)
{
    GtkWidget *focus = gtk_container_get_focus_child(GTK_CONTAINER(data));
    while (!VTE_IS_TERMINAL(focus)) {
        focus = gtk_container_get_focus_child(GTK_CONTAINER(focus));
    }
    vte_terminal_paste_clipboard(VTE_TERMINAL(focus));

    return TRUE;
}

static void setup_palette()
{ 
    gdk_rgba_parse(&palette[0], "#1B1D1E");
    gdk_rgba_parse(&palette[1], "#F92672");
    gdk_rgba_parse(&palette[2], "#82B414");
    gdk_rgba_parse(&palette[3], "#FD971F");
    gdk_rgba_parse(&palette[4], "#56C2D6");
    gdk_rgba_parse(&palette[5], "#8C54FE");
    gdk_rgba_parse(&palette[6], "#465457");
    gdk_rgba_parse(&palette[7], "#CCCCC6");
    gdk_rgba_parse(&palette[8], "#505354");
    gdk_rgba_parse(&palette[9], "#FF5995");
    gdk_rgba_parse(&palette[10], "#B6E354");
    gdk_rgba_parse(&palette[11], "#FEED6C");
    gdk_rgba_parse(&palette[12], "#8CEDFF");
    gdk_rgba_parse(&palette[13], "#9E6FFE");
    gdk_rgba_parse(&palette[14], "#899CA1");
    gdk_rgba_parse(&palette[15], "#F8F8F2");
}

int main(int argc, char *argv[])
{
    GtkWidget *win;
    GtkWidget *vte;
    GtkAccelGroup *accel_group;
    GClosure *closure;
    GError *error = NULL;

    gtk_init(&argc, &argv);

    // Settings
    font_desc = pango_font_description_from_string(FONT_SPEC);
    gdk_rgba_parse(&bg, BACKGROUND_COLOR);
    gdk_rgba_parse(&fg, FOREGROUND_COLOR);
    //setup_palette();

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 400);
    gtk_widget_override_font(win, font_desc);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), NULL);

    accel_group = gtk_accel_group_new();
    closure = g_cclosure_new(G_CALLBACK(split_horizontal), win, 0);
    gtk_accel_group_connect(accel_group,
                            GDK_KEY_bar,
                            GDK_CONTROL_MASK,
                            0,
                            closure);
    closure = g_cclosure_new(G_CALLBACK(split_vertical), win, 0);
    gtk_accel_group_connect(accel_group,
                            GDK_KEY_underscore,
                            GDK_CONTROL_MASK,
                            0,
                            closure);
    closure = g_cclosure_new(G_CALLBACK(copy_clipboard), win, 0);
    gtk_accel_group_connect(accel_group,
                            GDK_KEY_c,
                            GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                            0,
                            closure);
    closure = g_cclosure_new(G_CALLBACK(paste_clipboard), win, 0);
    gtk_accel_group_connect(accel_group,
                            GDK_KEY_v,
                            GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                            0,
                            closure);
    gtk_window_add_accel_group(GTK_WINDOW(win), accel_group);

    vte = new_term(win, &error);
    if (error != NULL)
    {
        fprintf(stderr, "Error: %s\n", error->message);
        g_error_free(error);
    }
    else
    {
        gtk_container_add(GTK_CONTAINER(win), vte);
        gtk_widget_show(vte);
        g_object_set(vte, "has-focus", TRUE, NULL);
    }

    gtk_widget_show_all(win);
    gtk_main();
}
