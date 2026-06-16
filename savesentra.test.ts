import { describe, it, expect } from "vitest";

// Denomination detection logic mirrored from savesentra_iot.ino
function detectDenomination(steps: number): number | null {
  if (steps >= 4990 && steps <= 5080) return 5;
  if (steps >= 5090 && steps <= 5200) return 10;
  return null;
}

describe("denomination detection", () => {
  it("identifies a 5 AED note", () => {
    expect(detectDenomination(5035)).toBe(5);
  });

  it("identifies a 10 AED note", () => {
    expect(detectDenomination(5145)).toBe(10);
  });

  it("returns null for an invalid note size", () => {
    expect(detectDenomination(1000)).toBeNull();
  });

  it("returns null on boundary below 5 AED range", () => {
    expect(detectDenomination(4989)).toBeNull();
  });

  it("returns null on boundary above 10 AED range", () => {
    expect(detectDenomination(5201)).toBeNull();
  });
});
