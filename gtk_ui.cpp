#include <gtk/gtk.h>
#include <string.h>
#include "find_file.h"

GtkWidget* directory_entry;
GtkWidget* include_pattern_entry;
GtkWidget* search_pattern_entry;
GtkWidget* search_pattern_type;

GtkWidget* search_result_list;

static void file_link_clicked(GtkLinkButton* link, gpointer user_data) {
    const char* uri = gtk_link_button_get_uri(link);

    g_app_info_launch_default_for_uri(uri, NULL, NULL);
    // TODO: Show possible error to user
}

static void search_button_clicked(GtkButton* button, gpointer user_data) {
    const char* include_pattern = gtk_entry_get_text(GTK_ENTRY(include_pattern_entry));
    const char* String = gtk_entry_get_text(GTK_ENTRY(search_pattern_entry));
    const char* path = gtk_entry_get_text(GTK_ENTRY(directory_entry));
    int path_len = strlen(path);

    recursive_directory_iterator dir_iterator = {};
    RecursiveDirectoryIteratorStart(&dir_iterator, path, path_len);
    
    int BufferSize = 32 * 1024 * 1024;
    char* FileBuffer = (char*)malloc(BufferSize);

    string_builder FilePath = {};
    StringBuilderInit(&FilePath, "", 0);

    string_builder URIBuilder = {};

    int StringLength = strlen(String);
    do
    {
        const char* FileName = RecursiveDirectoryIteratorFileName(&dir_iterator);
        if (RecursiveDirectoryIteratorIsDir(&dir_iterator))
        {
        }
        else
        {
            if (GlobMatch(include_pattern, FileName))
            {
                StringBuilderCopy(&FilePath, &dir_iterator.Path);
                StringBuilderAppend(&FilePath, FileName, strlen(FileName));

                // TODO: File links
                // TODO: Check if binary file

                FILE* File = fopen(FilePath.String, "rb");
                if (File)
                {
                    fseek(File, 0, SEEK_END);
                    size_t FileSize = ftell(File);
                    fseek(File, 0, SEEK_SET);

                    if (FileSize > BufferSize)
                    {
                        BufferSize = (FileSize + 4096 - 1) - FileSize % 4096;
                        FileBuffer = (char*)realloc(FileBuffer, BufferSize);
                    }

                    // TODO: Display error
                    fread(FileBuffer, FileSize, 1, File);
                    fclose(File);

                    bool Matches = false;
                    int LineStart = 0;
                    int LineNum = 1;
                    for (int I = 0; I < FileSize; I++) 
                    {
                        if (FileBuffer[I] == '\n')
                        {
                            LineNum++;
                            LineStart = I + 1;
                        }
                        else if (FileBuffer[I] == '\r')
                        {
                            if (I + 1 < FileSize && FileBuffer[I + 1] == '\n')
                            {
                                I++;
                            }
                            LineNum++;
                            LineStart = I + 1;
                        }
                        else
                        {
                            Matches = true;
                            for (int S = 0; S < StringLength; S++)
                            {
                                if (FileSize <= (I + S) || FileBuffer[I + S] != String[S])
                                {
                                    Matches = false;
                                    break;
                                }
                            }

                            if (Matches)
                            {
                                int EndOfLine = I;
                                while (EndOfLine < FileSize && FileBuffer[EndOfLine] != '\r' && FileBuffer[EndOfLine] != '\n')
                                {
                                    EndOfLine++;
                                }

                                StringBuilderInit(&URIBuilder, "file://", strlen("file://"));
                                StringBuilderAppend(&URIBuilder, FilePath.String, FilePath.Length);

                                GtkWidget* file_link = gtk_link_button_new_with_label(URIBuilder.String, FilePath.String + path_len + 1);
                                gtk_widget_set_halign(file_link, GTK_ALIGN_START);
                                gtk_container_add(GTK_CONTAINER(search_result_list), file_link);
                            }
                        }
                    }
                }
                else
                {
                    printf("Failed to open file: '%s'\n", FilePath.String);
                }
            }
        }
    }
    while (RecursiveDirectoryIteratorNext(&dir_iterator));


    free(FileBuffer);

    gtk_widget_show_all(search_result_list);
}

static void app_activate(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "DMQ Search");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    // Build UI
    directory_entry       = gtk_entry_new();
    include_pattern_entry = gtk_entry_new();
    search_pattern_entry  = gtk_entry_new();

    search_result_list = gtk_list_box_new();

    search_pattern_type   = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(search_pattern_type), "text", "Text");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(search_pattern_type), "glob", "Glob");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(search_pattern_type), "regex", "Regex");
    gtk_combo_box_set_active(GTK_COMBO_BOX(search_pattern_type), 0);

    GtkWidget* browse_button = gtk_button_new_with_label("Browse");
    GtkWidget* search_button = gtk_button_new_with_label("Search");

    gtk_widget_set_hexpand(directory_entry, TRUE);
    gtk_widget_set_hexpand(search_pattern_entry, TRUE);


    GtkWidget* grid = gtk_grid_new();

    // row 0
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Directory"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), directory_entry,            1, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), browse_button,              3, 0, 1, 1);

    // row 1
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Include"),   0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), include_pattern_entry,      1, 1, 3, 1);

    // row 2
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Pattern"),   0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), search_pattern_type,        1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), search_pattern_entry,       2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), search_button,              3, 2, 1, 1);

    // row 4
    GtkWidget* list_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(list_scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(list_scroll), search_result_list);
    gtk_grid_attach(GTK_GRID(grid), list_scroll,                0, 4, 4, 1);

    gtk_container_add(GTK_CONTAINER(window), grid);

    // Setup signals
    g_signal_connect(search_button, "clicked", G_CALLBACK(search_button_clicked), NULL);

    gtk_widget_show_all(window);
}



int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);

    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
