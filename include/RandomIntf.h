#pragma once

#include <cstdint>

/**
 * Interface for random number generation
 * Abstracts random number generation for band selection and other features
 */
class RandomIntf {
public:
    virtual ~RandomIntf() = default;
    
    /**
     * Initialize/seed the random number generator
     * @param seed Seed value for the generator
     */
    virtual void seed(uint32_t seed) = 0;
    
    /**
     * Generate a random integer in range [0, max)
     * @param max Upper bound (exclusive)
     * @return Random integer from 0 to max-1
     */
    virtual int randInt(int max) = 0;
    
    /**
     * Generate a random integer in range [min, max]
     * @param min Lower bound (inclusive)
     * @param max Upper bound (inclusive)
     * @return Random integer from min to max
     */
    virtual int randRange(int min, int max) = 0;
    
    /**
     * Generate a random float in range [0.0, 1.0)
     * @return Random float from 0.0 to 1.0 (exclusive)
     */
    virtual float randFloat() = 0;
};