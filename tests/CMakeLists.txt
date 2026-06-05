include(ECMAddTests)

ecm_add_test(importernametest.cpp
    TEST_NAME importernametest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
