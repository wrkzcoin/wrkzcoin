add_subdirectory(ed25519)
add_subdirectory(sha3)

set_property(TARGET ed25519 PROPERTY FOLDER "external")
set_property(TARGET sha3 PROPERTY FOLDER "external")