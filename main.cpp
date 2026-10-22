#include "parser.hpp"
#include "scanner.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string grammar_file = "Chocopy_Gramatica.txt";
    std::string source_file = "";

    if (argc > 1) {
        source_file = argv[1];
    }
    if (argc > 2) {
        grammar_file = argv[2];
    }

    try {
        // Inicializamos siempre la gramática primero para cargar la simbología
        Grammar grammar = Grammar::fromFile(grammar_file);
        LR1Builder builder(grammar);

        std::string table_cache = "tabla_chocopy.dat";
        if (!builder.loadTable(table_cache)) {
            std::cout << "[INFO] Generando coleccion canonica y tabla LR(1)...\n";
            builder.first();
            builder.coleccionCanonica();
            if (!builder.llenarTablas()) {
                std::cerr << "[ADVERTENCIA] Se detectaron conflictos en la tabla LR(1).\n";
            }
            builder.saveTable(table_cache);
            std::cout << "[INFO] Tabla guardada exitosamente en '" << table_cache << "'.\n";
        } else {
            std::cout << "[INFO] Tabla LR(1) cargada desde archivo de almacenamiento.\n";
            builder.first();
        }

        if (source_file.empty()) {
            std::cout << "\nUso: ./Compilador_Chocopy.exe <archivo_de_prueba.py>\n";
            return 0;
        }

        std::cout << "[INFO] Ejecutando Scanner sobre: " << source_file << "...\n";
        Scanner scanner(source_file);
        std::vector<Token> tokens = scanner.scan();

        if (scanner.hasError()) {
            std::cout << "\n[ERROR SCANNER] Se encontraron errores lexicos. "
                        "Se omite el analisis sintactico.\n";
            return 1;
        }

        std::cout << "[INFO] Iniciando analisis sintactico...\n";
        bool ok = builder.parseTokensWithTrace(tokens);

        
        if (ok) {
            std::cout << "\n[EXITO] Analisis finalizado sin errores.\n";
             
        } else {
            std::cout << "\n[CON ERRORES] El archivo contiene errores sintacticos.\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}