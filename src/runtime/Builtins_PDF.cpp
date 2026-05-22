#include "../Builtins.h"
#include "../RuntimeContext.h"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>

struct SimplePDF {
    std::string filename;
    std::vector<long> offsets;
    std::stringstream body;
    std::stringstream currentStream;
    int pageCount = 0;
    std::vector<int> pageIds;
    std::vector<int> contentIds;
    int catalogId, pagesId, fontId;

    void begin(const std::string& fname) {
        filename = fname;
        body.str(""); body.clear();
        currentStream.str(""); currentStream.clear();
        offsets.clear();
        pageIds.clear();
        contentIds.clear();
        pageCount = 0;
        body << "%PDF-1.4\n";
    }

    int reserveId() {
        offsets.push_back(0);
        return (int)offsets.size();
    }

    void startObject(int id) {
        offsets[id - 1] = (long)body.tellp();
        body << id << " 0 obj\n";
    }

    void addPage() {
        if (pageCount > 0) finalizePage();
        pageCount++;
        pageIds.push_back(reserveId());
        contentIds.push_back(reserveId());
        currentStream.str(""); currentStream.clear();
    }

    void finalizePage() {
        int cId = contentIds.back();
        startObject(cId);
        std::string s = currentStream.str();
        body << "<< /Length " << s.length() << " >>\nstream\n" << s << "\nendstream\nendobj\n";
    }

    void save() {
        if (pageCount == 0) addPage();
        finalizePage();

        catalogId = reserveId();
        pagesId = reserveId();
        fontId = reserveId();

        // 1. Catalog
        startObject(catalogId);
        body << "<< /Type /Catalog /Pages " << pagesId << " 0 R >>\nendobj\n";

        // 2. Pages Root
        startObject(pagesId);
        body << "<< /Type /Pages /Kids [";
        for (int id : pageIds) body << id << " 0 R ";
        body << "] /Count " << pageCount << " >>\nendobj\n";

        // 3. Font
        startObject(fontId);
        body << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Name /F1 >>\nendobj\n";

        // 4. Page Objects
        for (size_t i = 0; i < pageIds.size(); ++i) {
            startObject(pageIds[i]);
            body << "<< /Type /Page /Parent " << pagesId << " 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 " << fontId << " 0 R >> >> /Contents " << contentIds[i] << " 0 R >>\nendobj\n";
        }

        // 5. xref
        long startXref = (long)body.tellp();
        body << "xref\n0 " << (offsets.size() + 1) << "\n0000000000 65535 f \n";
        for (long off : offsets) {
            body << std::setw(10) << std::setfill('0') << off << " 00000 n \n";
        }

        // 6. trailer
        body << "trailer\n<< /Size " << (offsets.size() + 1) << " /Root " << catalogId << " 0 R >>\nstartxref\n" << startXref << "\n%%EOF";

        std::ofstream out(filename, std::ios::binary);
        out << body.str();
        out.close();
    }
};

static SimplePDF g_pdf;

void registerPDFBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("pdf_begin", Value::makeNativeFunction("pdf_begin", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            g_pdf.begin(args[0].toString());
            return Value();
        }));

    interp.defineGlobal("pdf_add_page", Value::makeNativeFunction("pdf_add_page", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            g_pdf.addPage();
            return Value();
        }));

    interp.defineGlobal("pdf_text", Value::makeNativeFunction("pdf_text", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("pdf_text() expects number size", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("pdf_text() expects number x", 0, ""); return Value(); }
            if (!args[2].isNumber()) { interp.runtimeError("pdf_text() expects number y", 0, ""); return Value(); }
            if (!args[3].isString()) { interp.runtimeError("pdf_text() expects string text", 0, ""); return Value(); }
            int size = (int)args[0].asNumber();
            int x = (int)args[1].asNumber();
            int y = (int)args[2].asNumber();
            std::string text = args[3].asString();
            g_pdf.currentStream << "BT /F1 " << size << " Tf " << x << " " << y << " Td (" << text << ") Tj ET\n";
            return Value();
        }));

    interp.defineGlobal("pdf_line", Value::makeNativeFunction("pdf_line", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("pdf_line() expects number x1", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("pdf_line() expects number y1", 0, ""); return Value(); }
            if (!args[2].isNumber()) { interp.runtimeError("pdf_line() expects number x2", 0, ""); return Value(); }
            if (!args[3].isNumber()) { interp.runtimeError("pdf_line() expects number y2", 0, ""); return Value(); }
            int x1 = (int)args[0].asNumber();
            int y1 = (int)args[1].asNumber();
            int x2 = (int)args[2].asNumber();
            int y2 = (int)args[3].asNumber();
            g_pdf.currentStream << x1 << " " << y1 << " m " << x2 << " " << y2 << " l s\n";
            return Value();
        }));

    interp.defineGlobal("pdf_save", Value::makeNativeFunction("pdf_save", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            g_pdf.save();
            return Value();
        }));
}
