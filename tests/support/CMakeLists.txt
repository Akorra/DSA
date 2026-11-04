# Test-support helpers and their self-tests.

dsa_add_test(selftest_tracked selftest_tracked.cpp)

# p0-tracked M2-M4 (learner): characterization tests of std::vector.
dsa_add_test(test_tracked test_tracked.cpp)

# 03_review adversarial tests (Claude, p0-tracked round 1).
dsa_add_test(review_tracked review_tracked.cpp)
