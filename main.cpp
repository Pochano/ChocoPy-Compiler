#include "parser.hpp"
#include "scanner.hpp" // --- CAMBIO: Incluimos el Scanner ---
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string grammar_file = "Chocopy_Gramatica.txt";
    std::string source_file = "";

    // --- CAMBIO: Ahora aceptamos el archivo de código fuente como primer argumento ---
    if (argc > 1) {
        source_file = argv[1];
    }
    if (argc > 2) {
        grammar_file = argv[2];
    }

    try {
        Grammar grammar = Grammar::fromFile(grammar_file);
        LR1Builder builder(grammar);

        // --- CAMBIO: Sistema de Caché para la Tabla LR(1) ---
        // Explicación: Calcular la tabla LR(1) tarda mucho. Aquí intentamos cargarla de un archivo (.dat).
        // Si no existe, la generamos desde cero y la guardamos para la próxima vez.
        std::string table_cache = "tabla_chocopy.dat";
        if (!builder.loadTable(table_cache)) {
            std::cout << "[INFO] Generando coleccion canonica y tabla LR(1)...\n";
            builder.first();
            builder.coleccionCanonica();
            if (!builder.llenarTablas()) {
                std::cerr << "[ADVERTENCIA] Se detectaron conflictos en la tabla LR(1).\n";
            }
            builder.saveTable(table_cache); // Guardamos la tabla
            std::cout << "[INFO] Tabla guardada exitosamente en '" << table_cache << "'.\n";
        } else {
            std::cout << "[INFO] Tabla LR(1) cargada desde archivo de almacenamiento.\n";
            builder.first(); // Necesario para imprimir FIRST si viene del caché
        }

        if (source_file.empty()) {
            std::cout << "\nUso: ./Compilador_Chocopy.exe <archivo_de_prueba.py>\n";
            return 0;
        }

        // --- CAMBIO: Ejecución del Scanner ---
        // Leemos el código y generamos la lista de tokens
        std::cout << "[INFO] Ejecutando Scanner sobre: " << source_file << "...\n";
        Scanner scanner(source_file);
        std::vector<Token> tokens = scanner.scan();

        if (scanner.hasError()) {
            std::cerr << "[ERROR SCANNER] Se encontraron errores lexicos.\n";
        }

        // --- CAMBIO: Ejecución del Parser ---
        // Le pasamos los tokens al parser usando la nueva función con seguimiento (Trace)
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