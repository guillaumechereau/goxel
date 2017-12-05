
include_directories(glew)
# TODO: Because of the custom imgui_user.inl extension, we are forced to compile imgui as a part of the project instead of static library.
# I propose that the extension be added as a pull request to imgui (which is actively maintained), and, after the changes are accepted
# Imgui be used as a git submodule, built as a static library, then linked to the goxel executable --Greg (GitHub: Algomorph)
#add_subdirectory(imgui)
add_subdirectory(inih)

