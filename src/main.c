#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

enum SplitOrientation {
    HORIZONTAL,
    VERTICAL
};

static void on_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void child_exited(VteTerminal *term, gint status, gpointer data)
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

static void on_terminal_window_title_changed(VteTerminal *term, gpointer data)
{
    gboolean has_focus;
    g_object_get(term, "has-focus", &has_focus, NULL);
    if (has_focus)
        gtk_window_set_title(GTK_WINDOW(data), vte_terminal_get_window_title(term));
}

static GtkWidget* new_term(GtkWidget *main_window, GError **error)
{
    char *argv[] = {"/usr/bin/bash", NULL};
    GPid pid;
    GtkWidget *vte = vte_terminal_new();
    vte_terminal_fork_command_full(VTE_TERMINAL(vte),
                                   VTE_PTY_DEFAULT,
                                   getenv("HOME"),
                                   argv,
                                   NULL,
                                   G_SPAWN_DEFAULT,
                                   NULL,
                                   NULL,
                                   &pid,
                                   error);
    g_signal_connect(vte,
                     "child-exited",
                     G_CALLBACK(child_exited),
                     NULL);
    g_signal_connect(vte,
                     "window-title-changed",
                     G_CALLBACK(on_terminal_window_title_changed),
                     main_window);
    return vte;
}

static void split(GtkWidget *main_window, GtkOrientation orientation)
{
    GtkWidget *focus = gtk_container_get_focus_child(GTK_CONTAINER(main_window));
    if (focus == NULL)
    {
        fprintf(stderr, "Error: No focus child.\n");
        return;
    }
    if (GTK_IS_PANED(focus))
    {
        while (!VTE_IS_TERMINAL(focus))
            focus = gtk_container_get_focus_child(GTK_CONTAINER(focus));
    }
    if (VTE_IS_TERMINAL(focus))
    {
        GtkWidget *vte;
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

        GtkAllocation alloc;
        gtk_widget_get_allocation(parent, &alloc);
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
            gtk_paned_set_position(GTK_PANED(paned), alloc.width / 2);
        else
            gtk_paned_set_position(GTK_PANED(paned), alloc.height / 2);

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

int main(int argc, char *argv[])
{
    GtkWidget *win;
    GtkWidget *vte;
    GtkAccelGroup *accel_group;
    GClosure *closure;
    GError *error = NULL;

    gtk_init(&argc, &argv);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 400);
    gtk_widget_show(win);
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

    gtk_main();
}
