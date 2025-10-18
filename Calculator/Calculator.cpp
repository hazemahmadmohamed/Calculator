#include <windows.h>
#include <string>
#include <cmath>
#include <stack>
#include <queue>
#include <map>
#include <sstream>
#include <iomanip>
#include <cctype>

// Fixed: Replaced constexpr with const for older C++ compatibility
const COLORREF OPERATOR_COLOR = RGB(255, 165, 0);    // Orange
const COLORREF NUMBER_COLOR = RGB(240, 240, 240);    // Light gray
const COLORREF FUNCTION_COLOR = RGB(173, 216, 230);  // Light blue
const COLORREF SPECIAL_COLOR = RGB(200, 200, 200);   // Gray for special buttons
const COLORREF EQUALS_COLOR = RGB(0, 255, 0);        // Green

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Application state
std::wstring currentExpression = L"";
HWND displayWindow;

// Token types for parser
enum class enTokenType { Number, Operator, Function, Parenthesis };

struct Token {
    enTokenType type;
    std::wstring value;
    double numericValue;
};

// Operator precedence mapping
const std::map<wchar_t, int> OPERATOR_PRECEDENCE = {
    {L'+', 1}, {L'-', 1},
    {L'*', 2}, {L'/', 2},
    {L'^', 3}
};

// Function prototypes
void UpdateDisplay();
double EvaluateExpression(const std::wstring& expr);
double ApplyOperator(double a, double b, wchar_t op);
double ApplyFunction(const std::wstring& func, double arg);
std::vector<Token> TokenizeExpression(const std::wstring& expr);
std::queue<Token> ConvertToRPN(const std::vector<Token>& tokens);
void AddButton(HWND parent, int x, int y, int width, int height,
    const wchar_t* text, int id, COLORREF bgColor);
void CreateCalculatorUI(HWND window);

// Display update handler
void UpdateDisplay() {
    SetWindowText(displayWindow, currentExpression.c_str());
}

// Main expression evaluation
double EvaluateExpression(const std::wstring& expr) {
    if (expr.empty()) return 0.0;

    try {
        auto tokens = TokenizeExpression(expr);
        auto rpnQueue = ConvertToRPN(tokens);
        std::stack<double> evaluationStack;

        while (!rpnQueue.empty()) {
            Token token = rpnQueue.front();
            rpnQueue.pop();

            switch (token.type) {
            case enTokenType::Number:
                evaluationStack.push(token.numericValue);
                break;

            case enTokenType::Operator: {
                if (evaluationStack.size() < 2)
                    throw std::runtime_error("Invalid expression");

                double b = evaluationStack.top();
                evaluationStack.pop();
                double a = evaluationStack.top();
                evaluationStack.pop();

                evaluationStack.push(ApplyOperator(a, b, token.value[0]));
                break;
            }

            case enTokenType::Function: {
                if (evaluationStack.empty())
                    throw std::runtime_error("Invalid function call");

                double arg = evaluationStack.top(); // ✅ السطر 98 بعد تصحيحه ضمن نطاق آمن
                evaluationStack.pop();
                evaluationStack.push(ApplyFunction(token.value, arg));
                break;
            }

            default:
                throw std::runtime_error("Unexpected token type");
            }
        }

        if (evaluationStack.size() != 1)
            throw std::runtime_error("Invalid expression format");

        return evaluationStack.top();
    }
    catch (...) {
        return NAN; // Return NaN for errors
    }
}

// Apply arithmetic operations
double ApplyOperator(double a, double b, wchar_t op) {
    switch (op) {
    case L'+': return a + b;
    case L'-': return a - b;
    case L'*': return a * b;
    case L'/':
        if (b == 0) throw std::runtime_error("Division by zero");
        return a / b;
    case L'^': return pow(a, b);
    default: throw std::runtime_error("Invalid operator");
    }
}

// Apply mathematical functions (in degrees)
double ApplyFunction(const std::wstring& func, double arg) {
    // Convert degrees to radians for trig functions
    double radians = arg * M_PI / 180.0;

    if (func == L"sin") return sin(radians);
    if (func == L"cos") return cos(radians);
    if (func == L"tan") return tan(radians);
    if (func == L"sqrt") {
        if (arg < 0) throw std::runtime_error("Negative sqrt argument");
        return sqrt(arg);
    }

    throw std::runtime_error("Unknown function");
}

// Tokenize input expression
std::vector<Token> TokenizeExpression(const std::wstring& expr) {
    std::vector<Token> tokens;
    size_t pos = 0;

    while (pos < expr.length()) {
        // Skip whitespace
        if (iswspace(expr[pos])) {
            pos++;
            continue;
        }

        // Handle numbers (including decimals and negatives)
        if (iswdigit(expr[pos]) || expr[pos] == L'.' ||
            (expr[pos] == L'-' && (tokens.empty() ||
                tokens.back().type == enTokenType::Operator ||
                tokens.back().type == enTokenType::Parenthesis ||
                tokens.back().value == L"("))) {

            size_t start = pos;
            if (expr[pos] == L'-') pos++;

            while (pos < expr.length() &&
                (iswdigit(expr[pos]) || expr[pos] == L'.')) {
                pos++;
            }

            std::wstring numStr = expr.substr(start, pos - start);
            tokens.push_back({
                enTokenType::Number,
                numStr,
                std::stod(numStr)
                });
            continue;
        }

        // Handle functions
        if (iswalpha(expr[pos])) {
            size_t start = pos;
            while (pos < expr.length() && iswalpha(expr[pos])) {
                pos++;
            }

            std::wstring func = expr.substr(start, pos - start);
            tokens.push_back({ enTokenType::Function, func, 0.0 });
            continue;
        }

        // Handle operators
        if (OPERATOR_PRECEDENCE.find(expr[pos]) != OPERATOR_PRECEDENCE.end()) {
            tokens.push_back({ enTokenType::Operator, std::wstring(1, expr[pos]), 0.0 });
            pos++;
            continue;
        }

        // Handle parentheses
        if (expr[pos] == L'(' || expr[pos] == L')') {
            tokens.push_back({
                enTokenType::Parenthesis,
                std::wstring(1, expr[pos]),
                0.0
                });
            pos++;
            continue;
        }

        // Unknown character
        throw std::runtime_error("Invalid character in expression");
    }

    return tokens;
}

// Convert tokens to Reverse Polish Notation (Shunting Yard algorithm)
std::queue<Token> ConvertToRPN(const std::vector<Token>& tokens) {
    std::queue<Token> outputQueue;
    std::stack<Token> operatorStack;

    for (const auto& token : tokens) {
        switch (token.type) {
        case enTokenType::Number:
            outputQueue.push(token);
            break;

        case enTokenType::Function:
            operatorStack.push(token);
            break;

        case enTokenType::Operator: {
            while (!operatorStack.empty() &&
                operatorStack.top().type != enTokenType::Parenthesis &&
                operatorStack.top().type != enTokenType::Function &&
                OPERATOR_PRECEDENCE.at(operatorStack.top().value[0]) >=
                OPERATOR_PRECEDENCE.at(token.value[0])) {
                outputQueue.push(operatorStack.top());
                operatorStack.pop();
            }
            operatorStack.push(token);
            break;
        }

        case enTokenType::Parenthesis:
            if (token.value == L"(") {
                operatorStack.push(token);
            }
            else {
                // Handle closing parenthesis
                while (!operatorStack.empty() &&
                    operatorStack.top().value != L"(") {
                    outputQueue.push(operatorStack.top());
                    operatorStack.pop();
                }

                if (operatorStack.empty()) {
                    throw std::runtime_error("Mismatched parentheses");
                }

                operatorStack.pop(); // Remove '('

                // Handle functions after parentheses
                if (!operatorStack.empty() &&
                    operatorStack.top().type == enTokenType::Function) {
                    outputQueue.push(operatorStack.top());
                    operatorStack.pop();
                }
            }
            break;

        default:
            break;
        }
    }

    // Process remaining operators
    while (!operatorStack.empty()) {
        if (operatorStack.top().value == L"(") {
            throw std::runtime_error("Mismatched parentheses");
        }
        outputQueue.push(operatorStack.top());
        operatorStack.pop();
    }

    return outputQueue;
}

// UI Functions
void AddButton(HWND parent, int x, int y, int width, int height,
    const wchar_t* text, int id, COLORREF bgColor) {
    HWND button = CreateWindow(
        L"BUTTON", text,
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, width, height,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr
    );
    SetWindowLongPtr(button, GWLP_USERDATA, static_cast<LONG_PTR>(bgColor));
}

void CreateCalculatorUI(HWND window) {
    const int buttonWidth = 60;
    const int buttonHeight = 40;
    const int margin = 10;
    const int displayHeight = 40;
    int x = margin;
    int y = margin;

    // Create display
    displayWindow = CreateWindow(
        L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_RIGHT | WS_BORDER | ES_READONLY,
        x, y, buttonWidth * 4 + margin * 3, displayHeight,
        window, nullptr, nullptr, nullptr
    );

    // Set display font
    HFONT font = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    SendMessage(displayWindow, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    y += displayHeight + margin;

    // Function buttons (light blue)
    AddButton(window, x, y, buttonWidth, buttonHeight, L"sin", 301, FUNCTION_COLOR);
    AddButton(window, x + (buttonWidth + margin), y, buttonWidth, buttonHeight, L"cos", 302, FUNCTION_COLOR);
    AddButton(window, x + 2 * (buttonWidth + margin), y, buttonWidth, buttonHeight, L"tan", 303, FUNCTION_COLOR);
    AddButton(window, x + 3 * (buttonWidth + margin), y, buttonWidth, buttonHeight, L"√", 304, FUNCTION_COLOR);
    AddButton(window, x + 4 * (buttonWidth + margin), y, buttonWidth, buttonHeight, L"x^y", 305, OPERATOR_COLOR);

    y += buttonHeight + margin;

    // Main button grid
    const wchar_t* buttonLabels[20] = {
        L"7", L"8", L"9", L"/",
        L"4", L"5", L"6", L"*",
        L"1", L"2", L"3", L"-",
        L"0", L".", L"=", L"+",
        L"(", L")", L"C", L"←"
    };

    const COLORREF buttonColors[20] = {
        NUMBER_COLOR, NUMBER_COLOR, NUMBER_COLOR, OPERATOR_COLOR,
        NUMBER_COLOR, NUMBER_COLOR, NUMBER_COLOR, OPERATOR_COLOR,
        NUMBER_COLOR, NUMBER_COLOR, NUMBER_COLOR, OPERATOR_COLOR,
        NUMBER_COLOR, NUMBER_COLOR, EQUALS_COLOR, OPERATOR_COLOR,
        SPECIAL_COLOR, SPECIAL_COLOR, SPECIAL_COLOR, SPECIAL_COLOR
    };

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int index = row * 4 + col;
            int xPos = x + col * (buttonWidth + margin);
            int yPos = y + row * (buttonHeight + margin);

            AddButton(window, xPos, yPos, buttonWidth, buttonHeight,
                buttonLabels[index], 100 + index, buttonColors[index]);
        }
    }
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateCalculatorUI(window);
        break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT drawInfo = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (drawInfo->CtlType == ODT_BUTTON) {
            COLORREF bgColor = static_cast<COLORREF>(
                GetWindowLongPtr(drawInfo->hwndItem, GWLP_USERDATA)
                );

            // Draw button background
            HBRUSH brush = CreateSolidBrush(bgColor);
            FillRect(drawInfo->hDC, &drawInfo->rcItem, brush);
            DeleteObject(brush);

            // Draw button text
            SetBkMode(drawInfo->hDC, TRANSPARENT);
            SetTextColor(drawInfo->hDC, RGB(0, 0, 0));
            wchar_t text[32];
            GetWindowText(drawInfo->hwndItem, text, 32);
            DrawText(drawInfo->hDC, text, -1, &drawInfo->rcItem,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Draw button border
            FrameRect(drawInfo->hDC, &drawInfo->rcItem,
                static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int buttonId = LOWORD(wParam);

        // Handle number and operator buttons
        if (buttonId >= 100 && buttonId < 120) {
            int index = buttonId - 100;
            const wchar_t* buttonValues[20] = {
                L"7", L"8", L"9", L"/",
                L"4", L"5", L"6", L"*",
                L"1", L"2", L"3", L"-",
                L"0", L".", L"=", L"+",
                L"(", L")", L"C", L"←"
            };

            // Handle special buttons
            if (index == 18) { // Clear
                currentExpression.clear();
            }
            else if (index == 19) { // Backspace
                if (!currentExpression.empty()) {
                    currentExpression.pop_back();
                }
            }
            else if (index == 14) { // Evaluate
                try {
                    double result = EvaluateExpression(currentExpression);
                    if (std::isnan(result)) {
                        currentExpression = L"Error";
                    }
                    else {
                        std::wstringstream stream;
                        stream << std::fixed << std::setprecision(10) << result;
                        currentExpression = stream.str();

                        // Remove trailing zeros
                        size_t decimalPos = currentExpression.find(L'.');
                        if (decimalPos != std::wstring::npos) {
                            size_t lastNonZero = currentExpression.find_last_not_of(L'0');
                            if (lastNonZero == decimalPos) {
                                currentExpression.erase(decimalPos);
                            }
                            else {
                                currentExpression.erase(lastNonZero + 1);
                            }
                        }
                    }
                }
                catch (...) {
                    currentExpression = L"Error";
                }
            }
            else { // Regular input
                currentExpression += buttonValues[index];
            }
            UpdateDisplay();
        }
        // Handle function buttons
        else if (buttonId >= 301 && buttonId <= 305) {
            switch (buttonId) 
            {
            case 301: currentExpression += L"sin("; break;
            case 302: currentExpression += L"cos("; break;
            case 303: currentExpression += L"tan("; break;
            case 304: currentExpression += L"sqrt("; break;
            case 305: currentExpression += L"^"; break;
            }
            UpdateDisplay();
        }
        break;
    }
    case WM_CHAR: {
        wchar_t ch = static_cast<wchar_t>(wParam);

        if (iswdigit(ch) || ch == L'+' || ch == L'-' || ch == L'x' || ch == L'/' || ch == L'.' || ch == L'^' || ch == L'(' || ch == L')') {
            currentExpression += ch;
            UpdateDisplay();
        }
        else if (ch == L'c' || ch == L'C') {
            currentExpression.clear();
            UpdateDisplay();
        }
        break;
    }

    case WM_KEYDOWN: {
        switch (wParam) {
        case VK_RETURN: { // Enter = evaluate
            try {
                double result = EvaluateExpression(currentExpression);
                if (std::isnan(result)) {
                    currentExpression = L"Error";
                }
                else {
                    std::wstringstream stream;
                    stream << std::fixed << std::setprecision(10) << result;
                    currentExpression = stream.str();

                    // Remove trailing zeros
                    size_t decimalPos = currentExpression.find(L'.');
                    if (decimalPos != std::wstring::npos) {
                        size_t lastNonZero = currentExpression.find_last_not_of(L'0');
                        if (lastNonZero == decimalPos) {
                            currentExpression.erase(decimalPos);
                        }
                        else {
                            currentExpression.erase(lastNonZero + 1);
                        }
                    }
                }
            }
            catch (...) {
                currentExpression = L"Error";
            }
            UpdateDisplay();
            break;
        }

        case VK_BACK: // Backspace
            if (!currentExpression.empty()) {
                currentExpression.pop_back();
                UpdateDisplay();
            }
            break;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
    return 0;
}

// Application entry point
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    const wchar_t CLASS_NAME[] = L"CalculatorClass";

    // Register window class
    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = CLASS_NAME;
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClass(&windowClass);

    // Create main window
    HWND window = CreateWindowEx(
        0, CLASS_NAME, L"Scientific Calculator",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 550,
        nullptr, nullptr, instance, nullptr
    );

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    // Message loop
    MSG message = {};
    while (GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}