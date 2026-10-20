set(MY_SRCS
    "main.cpp"
    "Eth_rmii/Src/eth_rmii.cpp"
    "Data_packet/Src/data_packet.cpp"
    )

set(MY_INCLUDES
    "."
    "Eth_rmii/Inc"
    "Data_packet/Inc"
    )

idf_component_register(
    SRCS ${MY_SRCS}
    INCLUDE_DIRS ${MY_INCLUDES}
)