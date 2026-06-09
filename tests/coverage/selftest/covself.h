#pragma once

// Two functions used to prove the coverage harness distinguishes executed from
// unexecuted code. The test calls only covselfCovered(); covselfUncovered() is
// never called, so the file's line coverage must land strictly between 0 and 1.
int covselfCovered(int n);
int covselfUncovered(int n);
