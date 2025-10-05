#include <gtest/gtest.h>
#include "Misc/EncryptionManager.h"
#include "Misc/EncryptionUtil.h"
#include "Misc/PasswordEncryptionArgon.h"
//#include "Public/Project.h"

TEST(CompressionTest, Accuracy)
{
    const std::string OriginalSalt = FEncryptionUtil::GenerateSecureSalt(256);

    auto start = std::chrono::high_resolution_clock::now();
    const std::vector<uint8_t> OriginalBytes = FEncryptionUtil::StringToBytes(OriginalSalt);
    const std::vector<uint8_t> Compressed = FHuffmanCompressor::Compress(OriginalBytes);
    const std::string ReversedSalt = FEncryptionUtil::BytesToString(FHuffmanCompressor::Decompress(Compressed));
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(OriginalSalt == ReversedSalt);
    EXPECT_LT(duration.count(), 100);
}

TEST(EncryptionTestArgon, Good)
{
	const std::string CorrectString = "MyT4STStringu";

	auto start = std::chrono::high_resolution_clock::now();
	std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	const std::string PassHash = Encryptor->HashPassword(CorrectString);
	const bool bIsEqualTrue = Encryptor->VerifyPassword(PassHash, CorrectString);
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	EXPECT_TRUE(bIsEqualTrue == true);
	EXPECT_LT(duration.count(), 500);
}

TEST(EncryptionTestArgon, Bad)
{
	const std::string CorrectString = "MyT4STStringu";
	const std::string IncorrectString = "STStringu";

	auto start = std::chrono::high_resolution_clock::now();
	std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	const std::string PassHash = Encryptor->HashPassword(CorrectString);
	const bool bIsEqualFalse = Encryptor->VerifyPassword(PassHash, IncorrectString);
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	EXPECT_TRUE(bIsEqualFalse == false);
	EXPECT_LT(duration.count(), 500);
}

TEST(EncryptionTestCustom, Good)
{
	const std::string CorrectString = "MyT4STStringu";
	const std::string IncorrectString = "STStringu";

	const std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(32);

	auto start = std::chrono::high_resolution_clock::now();
	const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(CorrectString, SecureSalt);
	const std::string PassEncrypt2 = FEncryptionUtil::EncryptDataCustom(CorrectString, SecureSalt);
	const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);
	const std::string PassDecrypt2 = FEncryptionUtil::DecryptDataCustom(PassEncrypt2, SecureSalt);
	const bool bIsCryptographySuccessful = (CorrectString == PassDecrypt) && (PassDecrypt == PassDecrypt2);
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	EXPECT_TRUE(bIsCryptographySuccessful == true);
	EXPECT_LT(duration.count(), 200);
}

TEST(EncryptionTestCustom, Bad)
{
	const std::string CorrectString = "MyT4STStringu";
	const std::string IncorrectString = "STStringu";

	const std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(32);

	const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(CorrectString, SecureSalt);
	const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(IncorrectString, SecureSalt);
	const bool bIsCryptographySuccessful = (CorrectString == PassDecrypt);

	EXPECT_TRUE(bIsCryptographySuccessful == false);
}

TEST(EncryptionTestCustom, MassTest)
{
	std::vector<std::string> Inputs = {
		"MyT4STStringu",
		"Different123!",
		"AAAAAAAAAAAAA",
		"TestMessage00",
		"XYZ123456789A"
	};

	const std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(32);

	auto start = std::chrono::high_resolution_clock::now();

	for (std::string& Input : Inputs)
	{
		const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);;
		const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);

		EXPECT_TRUE(Input == PassDecrypt);

		std::cout << Input << "->" << PassEncrypt << std::endl;
	}

	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	
	EXPECT_LT(duration.count(), 200);
}



// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Policz liczbę bitów = 1 (bez __builtin_popcount)
int CountBits(uint8_t byte)
{
    int count = 0;
    while (byte) {
        count += byte & 1;
        byte >>= 1;
    }
    return count;
}

// Konwertuj na hex
std::string ToHex(const std::string& data)
{
    std::stringstream ss;
    for (unsigned char c : data) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c << " ";
    }
    return ss.str();
}

// Wyświetl z escape sequences
std::string ToReadable(const std::string& data)
{
    std::stringstream ss;
    ss << "\"";
    for (unsigned char c : data) {
        if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
            ss << c;
        }
        else {
            ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    ss << "\"";
    return ss.str();
}

// Policz różne bajty
int DifferentBytes(const std::string& a, const std::string& b)
{
    int count = 0;
    size_t minSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < minSize; i++) {
        if (a[i] != b[i]) count++;
    }
    return count;
}

// Policz różne bity
int DifferentBits(const std::string& a, const std::string& b)
{
    int bits = 0;
    size_t minSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < minSize; i++) {
        uint8_t xorResult = static_cast<uint8_t>(a[i] ^ b[i]);
        bits += CountBits(xorResult);
    }
    return bits;
}

// Oblicz entropię
double CalculateEntropy(const std::string& data)
{
    if (data.empty()) return 0.0;

    std::map<uint8_t, int> freq;
    for (unsigned char c : data) {
        freq[static_cast<uint8_t>(c)]++;
    }

    double entropy = 0.0;
    for (const auto& p : freq) {
        double prob = (double)p.second / data.size();
        entropy -= prob * log2(prob);
    }
    return entropy;
}

// Policz czytelne znaki
int ReadableChars(const std::string& data)
{
    int count = 0;
    for (unsigned char c : data) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z')) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// TEST 1: Correctness - Decrypt = Original
// ============================================================================
TEST(EncryptionSecurity, DecryptionCorrectness)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::vector<std::string> testInputs = {
        "MyT4STStringu",
        "Short",
        "A",
        "",
        "Very Long Test Message With Multiple Words And Numbers 1234567890",
        "Special!@#$%^&*()_+-=[]{}|;:',.<>?/",
        std::string("\x00\x01\x02\xFF", 4), // Binary data
    };

    std::cout << "\n=== CORRECTNESS TEST ===" << std::endl;

    int passed = 0;
    for (const auto& Input : testInputs) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);
        const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);

        if (Input == PassDecrypt) {
            std::cout << "✓ ";
            passed++;
        }
        else {
            std::cout << "✗ ";
        }

        std::cout << (Input.empty() ? "(empty)" : ToReadable(Input).substr(0, 30)) << std::endl;

        EXPECT_EQ(Input, PassDecrypt)
            << "Failed for input: " << ToReadable(Input);
    }

    std::cout << "Passed: " << passed << "/" << testInputs.size() << std::endl;
}

// ============================================================================
// TEST 2: Pattern Detection - "590" Problem
// ============================================================================
TEST(EncryptionSecurity, PatternDetection)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== PATTERN DETECTION TEST ===" << std::endl;

    std::vector<std::string> inputs = {
        "MyT4STStringu",
        "Different123!",
        "AAAAAAAAAAAAA",
        "TestMessage00",
        "XYZ123456789A",
        "abcdefghijklm",
        "0000000000000",
    };

    std::map<std::string, int> endPatterns;
    std::set<std::string> allEncrypted;

    for (const auto& Input : inputs) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);

        allEncrypted.insert(PassEncrypt);

        // Sprawdź ostatnie 3 bajty
        if (PassEncrypt.size() >= 3) {
            std::string ending = PassEncrypt.substr(PassEncrypt.size() - 3);
            endPatterns[ending]++;

            std::cout << "Input: " << std::setw(15) << std::left << Input
                << " | End: " << ToReadable(ending) << std::endl;
        }
    }

    std::cout << "\n--- Analysis ---" << std::endl;
    std::cout << "Unique encrypted outputs: " << allEncrypted.size()
        << "/" << inputs.size() << std::endl;
    std::cout << "Unique endings: " << endPatterns.size()
        << "/" << inputs.size() << std::endl;

    // Sprawdź czy jakiś pattern się powtarza
    int maxRepeat = 0;
    std::string mostCommon;
    for (const auto& p : endPatterns) {
        if (p.second > maxRepeat) {
            maxRepeat = p.second;
            mostCommon = p.first;
        }
    }

    if (maxRepeat > 1) {
        std::cout << "⚠️  Pattern found: " << ToReadable(mostCommon)
            << " appears " << maxRepeat << " times" << std::endl;
    }
    else {
        std::cout << "✅ No repeating patterns detected" << std::endl;
    }

    EXPECT_EQ(allEncrypted.size(), inputs.size())
        << "Different inputs should produce different outputs";

    EXPECT_LE(maxRepeat, 2)
        << "Pattern '" << ToReadable(mostCommon) << "' repeats " << maxRepeat << " times";
}

// ============================================================================
// TEST 3: Avalanche Effect
// ============================================================================
TEST(EncryptionSecurity, AvalancheEffect)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== AVALANCHE EFFECT TEST ===" << std::endl;

    std::string original = "MyT4STStringu";

    const std::string encOriginal = FEncryptionUtil::EncryptDataCustom(original, SecureSalt);

    std::cout << "Original: \"" << original << "\"" << std::endl;
    std::cout << "Encrypted: " << ToReadable(encOriginal) << std::endl;
    std::cout << "\nChanging each character (flipping 1 bit):" << std::endl;

    std::vector<float> changePercentages;

    for (size_t i = 0; i < original.size(); i++) {
        std::string modified = original;
        modified[i] ^= 0x01; // Flip 1 bit

        const std::string encModified = FEncryptionUtil::EncryptDataCustom(modified, SecureSalt);

        int diffBytes = DifferentBytes(encOriginal, encModified);
        int diffBits = DifferentBits(encOriginal, encModified);
        float bytePercent = (float)diffBytes / encOriginal.size() * 100;
        float bitPercent = (float)diffBits / (encOriginal.size() * 8) * 100;

        changePercentages.push_back(bitPercent);

        std::cout << "Position " << std::setw(2) << i << ": "
            << diffBytes << "/" << encOriginal.size() << " bytes ("
            << std::fixed << std::setprecision(1) << bytePercent << "%), "
            << diffBits << " bits (" << bitPercent << "%)" << std::endl;
    }

    // Oblicz średnią
    float avgPercent = 0;
    for (float p : changePercentages) {
        avgPercent += p;
    }
    avgPercent /= changePercentages.size();

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Average bit change: " << std::fixed << std::setprecision(1)
        << avgPercent << "%" << std::endl;

    if (avgPercent >= 45 && avgPercent <= 55) {
        std::cout << "✅ EXCELLENT: Ideal avalanche effect (~50%)" << std::endl;
    }
    else if (avgPercent >= 40 && avgPercent <= 60) {
        std::cout << "✓  GOOD: Strong avalanche effect" << std::endl;
    }
    else if (avgPercent >= 30) {
        std::cout << "⚠️  WEAK: Poor avalanche effect" << std::endl;
    }
    else {
        std::cout << "🚨 CRITICAL: Very weak avalanche effect!" << std::endl;
    }

    EXPECT_GT(avgPercent, 40.0f)
        << "Avalanche effect too weak: " << avgPercent << "%";
    EXPECT_LT(avgPercent, 60.0f)
        << "Avalanche effect suspicious: " << avgPercent << "%";
}

// ============================================================================
// TEST 4: Deterministic Check
// ============================================================================
TEST(EncryptionSecurity, DeterministicEncryption)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== DETERMINISTIC TEST ===" << std::endl;

    std::string Input = "MyT4STStringu";

    const std::string enc1 = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);
    const std::string enc2 = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);

    bool isDeterministic = (enc1 == enc2);

    std::cout << "Same input encrypted twice:" << std::endl;
    std::cout << "  Result 1 == Result 2: " << (isDeterministic ? "YES" : "NO") << std::endl;

    if (isDeterministic) {
        std::cout << "⚠️  Deterministic encryption (no IV/salt per encryption)" << std::endl;
        std::cout << "   This is OK for simple encryption with salt, but:" << std::endl;
        std::cout << "   - Same plaintext + same salt = same ciphertext" << std::endl;
        std::cout << "   - Consider adding random IV for production" << std::endl;
    }
    else {
        std::cout << "✅ Non-deterministic (uses random IV)" << std::endl;
    }
}

// ============================================================================
// TEST 5: Entropy Test
// ============================================================================
TEST(EncryptionSecurity, EntropyTest)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== ENTROPY TEST ===" << std::endl;

    std::vector<std::pair<std::string, std::string>> tests = {
        {"Short text", "MyT4STStringu"},
        {"Repeated chars", "AAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
        {"Sequential", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
        {"Long mixed", "The quick brown fox jumps over the lazy dog 1234567890!@#$%"},
    };

    for (const auto& test : tests)
    {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(test.second, SecureSalt);

        double entropy = CalculateEntropy(PassEncrypt);
        int readable = ReadableChars(PassEncrypt);
        float readablePercent = (float)readable / PassEncrypt.size() * 100;

        std::cout << test.first << ":" << std::endl;
        std::cout << "  Entropy: " << std::fixed << std::setprecision(2)
            << entropy << " bits/byte (max 8.0)" << std::endl;
        std::cout << "  Readable chars: " << readable << "/" << PassEncrypt.size()
            << " (" << std::setprecision(1) << readablePercent << "%)" << std::endl;

        if (entropy > 7.2) {
            std::cout << "  ✅ Excellent randomness" << std::endl;
        }
        else if (entropy > 6.0) {
            std::cout << "  ✓  Good randomness" << std::endl;
        }
        else if (entropy > 4.0) {
            std::cout << "  ⚠️  Moderate randomness" << std::endl;
        }
        else {
            std::cout << "  🚨 Poor randomness" << std::endl;
        }

        if (test.second.size() >= 20) {

            EXPECT_NEAR(entropy, 6.0, 0.2)
                << "Entropy too low for " << test.first;
        }
    }
}

// ============================================================================
// TEST 6: Known Plaintext Attack Simulation
// ============================================================================
TEST(EncryptionSecurity, KnownPlaintextResistance)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== KNOWN PLAINTEXT ATTACK TEST ===" << std::endl;

    std::vector<std::string> knownPlaintexts = {
        "MyT4STStringu",
        "AAAAAAAAAAAAA",
        "TestMessage01",
    };

    std::vector<std::string> ciphertexts;

    for (const auto& plain : knownPlaintexts) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(plain, SecureSalt);
        ciphertexts.push_back(PassEncrypt);
    }

    std::cout << "Attempting to recover key via XOR attack..." << std::endl;

    std::vector<std::string> possibleKeys;
    for (size_t i = 0; i < knownPlaintexts.size(); i++) {
        std::string possibleKey;
        size_t minLen = std::min(knownPlaintexts[i].size(), ciphertexts[i].size());

        for (size_t j = 0; j < minLen; j++) {
            possibleKey += (ciphertexts[i][j] ^ knownPlaintexts[i][j]);
        }
        possibleKeys.push_back(possibleKey);

        std::cout << "Possible key " << (i + 1) << ": "
            << ToReadable(possibleKey) << std::endl;
    }

    bool allSame = true;
    for (size_t i = 1; i < possibleKeys.size(); i++) {
        if (possibleKeys[i] != possibleKeys[0]) {
            allSame = false;
            break;
        }
    }

    if (allSame && !possibleKeys.empty()) {
        std::cout << "🚨 CRITICAL: Simple XOR cipher detected!" << std::endl;
        std::cout << "   Recovered key: " << ToReadable(possibleKeys[0]) << std::endl;
        std::cout << "   This encryption is EASILY breakable!" << std::endl;

        FAIL() << "Simple XOR cipher - NOT SECURE FOR PRODUCTION";
    }
    else {
        std::cout << "✅ Keys differ - resistant to simple XOR attack" << std::endl;
    }
}

// ============================================================================
// TEST 7: Frequency Analysis
// ============================================================================
TEST(EncryptionSecurity, FrequencyAnalysis)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== FREQUENCY ANALYSIS TEST ===" << std::endl;

    std::string plaintext = "EEEEEEEEEEEEE TTTTTTTTTT AAAAAAAAAA OOOOOOO";

    const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(plaintext, SecureSalt);

    std::map<uint8_t, int> freq;
    for (unsigned char c : PassEncrypt) {
        freq[static_cast<uint8_t>(c)]++;
    }

    std::vector<std::pair<uint8_t, int>> sortedFreq(freq.begin(), freq.end());
    std::sort(sortedFreq.begin(), sortedFreq.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::cout << "Top 3 most frequent bytes:" << std::endl;
    for (size_t i = 0; i < std::min(size_t(3), sortedFreq.size()); i++) {
        float percent = (float)sortedFreq[i].second / PassEncrypt.size() * 100;
        std::cout << "  0x" << std::hex << std::setw(2) << std::setfill('0')
            << (int)sortedFreq[i].first << ": " << std::dec
            << sortedFreq[i].second << " times ("
            << std::fixed << std::setprecision(1) << percent << "%)" << std::endl;
    }

    float maxPercent = (float)sortedFreq[0].second / PassEncrypt.size() * 100;

    if (maxPercent > 15) {
        std::cout << "⚠️  WARNING: Most frequent byte appears " << maxPercent
            << "% - vulnerable to frequency analysis" << std::endl;
    }
    else {
        std::cout << "✅ Good distribution - resistant to frequency analysis" << std::endl;
    }

    EXPECT_LT(maxPercent, 20.0f)
        << "Frequency analysis possible - byte appears " << maxPercent << "% of time";
}

// ============================================================================
// TEST 8: Performance Test
// ============================================================================
TEST(EncryptionSecurity, PerformanceTest)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n=== PERFORMANCE TEST ===" << std::endl;

    std::string testData = "Test Message For Performance";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(testData, SecureSalt);
        const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "1000 encrypt/decrypt cycles: " << duration.count() << "ms" << std::endl;
    std::cout << "Average per cycle: " << std::fixed << std::setprecision(3)
        << (duration.count() / 1000.0) << "ms" << std::endl;

    if (duration.count() < 100) {
        std::cout << "✅ BLAZING FAST!" << std::endl;
    }
    else if (duration.count() < 500) {
        std::cout << "✓  FAST" << std::endl;
    }
    else {
        std::cout << "⚠️  Slow - consider optimization" << std::endl;
    }
}

// ============================================================================
// TEST 9: Final Security Report
// ============================================================================
TEST(EncryptionSecurity, FinalSecurityReport)
{
    std::string SecureSalt = FEncryptionUtil::GenerateSecureSalt(64);

    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "           FINAL ENCRYPTION SECURITY REPORT" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    std::vector<std::string> inputs = {
        "MyT4STStringu",
        "Test123",
        "A",
        "AAAAAAAAAAAAAA",
        "The quick brown fox",
    };

    int totalTests = 0;
    int passedTests = 0;

    // Test 1: Correctness
    std::cout << "\n1. CORRECTNESS TEST" << std::endl;
    bool correctnessPass = true;
    for (const auto& Input : inputs) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);
        const std::string PassDecrypt = FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);
        if (Input != PassDecrypt) {
            correctnessPass = false;
        }
    }
    std::cout << (correctnessPass ? "   ✅ PASS" : "   ❌ FAIL")
        << " - All decryptions correct" << std::endl;
    totalTests++;
    if (correctnessPass) passedTests++;

    // Test 2: Avalanche
    std::cout << "\n2. AVALANCHE EFFECT TEST" << std::endl;
    std::string test1 = "MyT4STStringu";
    std::string test2 = "MyT4STStringa";
    const std::string enc1 = FEncryptionUtil::EncryptDataCustom(test1, SecureSalt);
    const std::string enc2 = FEncryptionUtil::EncryptDataCustom(test2, SecureSalt);

    int diffBits = DifferentBits(enc1, enc2);
    float bitPercent = (float)diffBits / (enc1.size() * 8) * 100;
    bool avalanchePass = (bitPercent >= 40 && bitPercent <= 60);

    std::cout << "   Bit change: " << std::fixed << std::setprecision(1)
        << bitPercent << "%" << std::endl;
    std::cout << (avalanchePass ? "   ✅ PASS" : "   ⚠️  WEAK")
        << " - " << (avalanchePass ? "Good" : "Poor") << " diffusion" << std::endl;
    totalTests++;
    if (avalanchePass) passedTests++;

    // Test 3: Entropy
    std::cout << "\n3. ENTROPY TEST" << std::endl;
    double entropy = CalculateEntropy(enc1);
    bool entropyPass = (entropy > 6.5);

    std::cout << "   Entropy: " << std::setprecision(2) << entropy << " bits/byte" << std::endl;
    std::cout << (entropyPass ? "   ✅ PASS" : "   ⚠️  LOW")
        << " - " << (entropyPass ? "Good" : "Moderate") << " randomness" << std::endl;
    totalTests++;
    if (entropyPass) passedTests++;

    // Test 4: Patterns
    std::cout << "\n4. PATTERN DETECTION TEST" << std::endl;
    std::set<std::string> uniqueOutputs;
    for (const auto& Input : inputs) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom(Input, SecureSalt);
        uniqueOutputs.insert(PassEncrypt);
    }
    bool patternPass = (uniqueOutputs.size() == inputs.size());

    std::cout << "   Unique outputs: " << uniqueOutputs.size() << "/" << inputs.size() << std::endl;
    std::cout << (patternPass ? "   ✅ PASS" : "   ❌ FAIL")
        << " - " << (patternPass ? "No" : "Found") << " repeating patterns" << std::endl;
    totalTests++;
    if (patternPass) passedTests++;

    // Test 5: Performance
    std::cout << "\n5. PERFORMANCE TEST" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        const std::string PassEncrypt = FEncryptionUtil::EncryptDataCustom("Test", SecureSalt);
        FEncryptionUtil::DecryptDataCustom(PassEncrypt, SecureSalt);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "   1000 encrypt/decrypt: " << duration.count() << "ms" << std::endl;
    std::cout << "   ✅ " << (duration.count() < 100 ? "BLAZING FAST" : "FAST") << std::endl;

    // Final Score
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FINAL SCORE: " << passedTests << "/" << totalTests << " tests passed" << std::endl;

    if (passedTests == totalTests) {
        std::cout << "✅ EXCELLENT - Strong encryption algorithm" << std::endl;
    }
    else if (passedTests >= totalTests * 0.75) {
        std::cout << "✓  GOOD - Suitable for most use cases" << std::endl;
    }
    else if (passedTests >= totalTests * 0.5) {
        std::cout << "⚠️  MODERATE - Consider improvements for production" << std::endl;
    }
    else {
        std::cout << "🚨 WEAK - NOT recommended for production use" << std::endl;
    }

    std::cout << std::string(70, '=') << std::endl;

    EXPECT_GE(passedTests, totalTests * 0.75)
        << "Security tests: " << passedTests << "/" << totalTests << " passed";
}
