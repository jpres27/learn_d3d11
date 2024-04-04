#if !defined(SHUFFLE_H)

void shuffle(Texture_Info **arr) {
    Texture_Info output[10];  // Create an output array to store the shuffled elements
    bool32 visited[10]; // Create a boolean array to keep track of visited elements
    // Initialize the visited array with zeros (false)
    for (int i = 0; i < 10; i++) {
        visited[i] = false;
    }
    // Perform the shuffle algorithm
    for (int i = 0; i < 10; i++) {
        int j = rand() % 10; // Generate a random index in the input array
        while (visited[j]) { // Find the next unvisited index
            j = rand() % 10;
        }
        output[i] = arr[j]; // Add the element at the chosen index to the output array
        visited[j] = true;     // Mark the element as visited
    }
    // Copy the shuffled elements back to the original array
    for (int i = 0; i < 10; i++) {
        arr[i] = output[i];
    }
}

#define SHUFFLE_H
#endif

