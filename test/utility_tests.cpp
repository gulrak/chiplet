//
// Created by schuemann on 24.01.25.
//
#include <doctest/doctest.h>

#include <chiplet/utility.hpp>

TEST_CASE("toOptionName - category transitions and kebab-case format")
{
    SUBCASE("Empty string remains empty") {
        CHECK(toOptionName("") == "");
    }

    SUBCASE("Pure lowercase is unchanged") {
        CHECK(toOptionName("simple") == "simple");
    }

    SUBCASE("Lower to upper: insert hyphen") {
        CHECK(toOptionName("myVar") == "my-var");
    }

    SUBCASE("Upper to lower: no direct hyphen, but next is lower anyway") {
        CHECK(toOptionName("UserID") == "user-id");
    }

    SUBCASE("Digit transitions: letter->digit and digit->letter") {
        CHECK(toOptionName("size10") == "size-10");
        CHECK(toOptionName("10Size") == "10-size");
        CHECK(toOptionName("size123other") == "size-123-other");
    }

    SUBCASE("Non-alphanumeric replaced by hyphens, collapsed if multiple") {
        CHECK(toOptionName("my_var__99Test") == "my-var-99-test");
        CHECK(toOptionName("my--special**string") == "my-special-string");
    }

    SUBCASE("Trim leading/trailing hyphens") {
        CHECK(toOptionName("-leading-") == "leading");
        CHECK(toOptionName("trailing-") == "trailing");
    }

    SUBCASE("Composite example") {
        CHECK(toOptionName("someMixedNumb3rCase__42 andMore")
              == "some-mixed-numb-3-r-case-42-and-more");
    }
}

TEST_CASE("toJsonKey - category transitions and camelCase format")
{
    SUBCASE("Empty string remains empty") {
        CHECK(toJsonKey("") == "");
    }

    SUBCASE("Pure lowercase is unchanged") {
        CHECK(toJsonKey("simple") == "simple");
    }

    SUBCASE("Lower to upper: insert upper") {
        CHECK(toJsonKey("myVar") == "myVar");
    }

    SUBCASE("Upper to lower: insert lower") {
        CHECK(toJsonKey("UserID") == "userId");
    }

    SUBCASE("Digit transitions: letter->digit and digit->letter") {
        CHECK(toJsonKey("size10") == "size10");
        CHECK(toJsonKey("10Size") == "10Size");
        CHECK(toJsonKey("size123other") == "size123Other");
    }

    SUBCASE("Non-alphanumeric replaced by following be an upper if letter, collapsed if multiple") {
        CHECK(toJsonKey("my_var__99Test") == "myVar99Test");
        CHECK(toJsonKey("my--special**string") == "mySpecialString");
    }

    SUBCASE("Trim leading/trailing special characters") {
        CHECK(toJsonKey("-leading-") == "leading");
        CHECK(toJsonKey("trailing-") == "trailing");
    }

    SUBCASE("Composite example") {
        CHECK(toJsonKey("someMixedNumb3rCase__42 andMore")
              == "someMixedNumb3RCase42AndMore");
    }
}
