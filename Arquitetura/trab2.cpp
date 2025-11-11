/* tomasulo.cpp
   Simulador do algoritmo de Tomasulo - ciclo a ciclo, com RS, ROB, Load/Store buffers.
   Compilar: g++ -std=c++17 -O2 -o tomasulo tomasulo.cpp
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <cctype>
#include <algorithm>

using namespace std;

// Configuráveis: tamanhos das estruturas
constexpr int MAX_INS = 256;
constexpr int REGISTRADORES = 32;
constexpr int TAM_MEM = 1024;
constexpr int RS_SOMA_COUNT = 6;
constexpr int RS_MUL_COUNT = 3;
constexpr int BUFFER_CARGA_COUNT = 4;
constexpr int BUFFER_ARM_COUNT = 4;
constexpr int TAM_ROB = 32;

// Latências (em ciclos)
constexpr int LAT_SOMA = 2;
constexpr int LAT_MUL = 10;
constexpr int LAT_DIV = 40;
constexpr int LAT_CARGA = 2;
constexpr int LAT_ARM = 2;

enum class TipoOp { ADD, SUB, MUL, DIV, LD, ST, NOP };

// Representação de uma instrução
struct Instr {
    TipoOp tipo = TipoOp::NOP;
    int dest = 0;
    int src1 = 0;
    int src2 = 0;
    int imm = 0;
    string texto;
};

// Estação de reserva para operações aritméticas
struct EstacaoReserva {
    bool ocupada = false;
    TipoOp op = TipoOp::NOP;
    int Vj = 0, Vk = 0;
    int Qj = 0, Qk = 0;
    int destRob = 0;
    bool executando = false;
    int ciclosExecRestantes = 0;
    int indice_rob = 0;
    string nome;
};

// Buffer para operações de memória (load/store)
struct BufferMem {
    bool ocupado = false;
    TipoOp op = TipoOp::NOP;
    int endereco = 0;
    int V = 0;
    int Q = 0;
    int indice_rob = 0;
    bool executando = false;
    int ciclosExecRestantes = 0;
    string nome;
};

// Entrada do ROB (Reorder Buffer)
struct EntradaROB {
    bool ocupada = false;
    TipoOp op = TipoOp::NOP;
    int dest = -1;
    bool pronta = false;
    int valor = 0;
    string texto_instr;
    int endereco_mem = 0;
    int valor_arm = 0;
};

// Registrador do banco de registradores
struct Registrador {
    int valor = 0;
    int tag = 0;
};

class SimuladorTomasulo {
private:
    // Fila de instruções e PC
    vector<Instr> filaInstr;
    int pc = 0;
    
    // Estruturas do algoritmo de Tomasulo
    array<EstacaoReserva, RS_SOMA_COUNT> RS_soma;
    array<EstacaoReserva, RS_MUL_COUNT> RS_mul;
    array<BufferMem, BUFFER_CARGA_COUNT> BufferCarga;
    array<BufferMem, BUFFER_ARM_COUNT> BufferArm;
    array<EntradaROB, TAM_ROB + 1> ROB;
    int cabeca_rob = 1, cauda_rob = 1;
    array<Registrador, REGISTRADORES> arquivo_reg;
    array<int, TAM_MEM> memoria;

public:
    SimuladorTomasulo() {
        memoria.fill(0);
    }


    const char* nomeOp(TipoOp t) const {
        switch(t) {
            case TipoOp::ADD: return "ADD";
            case TipoOp::SUB: return "SUB";
            case TipoOp::MUL: return "MUL";
            case TipoOp::DIV: return "DIV";
            case TipoOp::LD:  return "LD";
            case TipoOp::ST:  return "ST";
            default: return "NOP";
        }
    }

    void trim(string& s) {
        s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !isspace(ch);
        }));
        s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !isspace(ch);
        }).base(), s.end());
    }

    int indiceRegistrador(const string& tok) {
        if(tok.empty() || tolower(tok[0]) != 'r') return -1;
        return stoi(tok.substr(1));
    }

    TipoOp parseOp(const string& s) {
        string lower = s;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if(lower == "add") return TipoOp::ADD;
        if(lower == "sub") return TipoOp::SUB;
        if(lower == "mul") return TipoOp::MUL;
        if(lower == "div") return TipoOp::DIV;
        if(lower == "ld" || lower == "lda" || lower == "li") return TipoOp::LD;
        if(lower == "st" || lower == "sd") return TipoOp::ST;
        return TipoOp::NOP;
    }


    void carregarPrograma(const string& filename) {
        ifstream f(filename);
        if(!f) {
            cerr << "Erro ao abrir arquivo: " << filename << endl;
            exit(1);
        }

        string linha;
        filaInstr.clear();
        
        while(getline(f, linha)) {
            trim(linha);
            if(linha.empty() || linha[0] == '#') continue;
            
            Instr ins;
            ins.texto = linha;
            
            istringstream iss(linha);
            string op;
            iss >> op;
            
            ins.tipo = parseOp(op);
            
            // Parse para instruções aritméticas (ADD, SUB, MUL, DIV)
            if(ins.tipo == TipoOp::ADD || ins.tipo == TipoOp::SUB || 
               ins.tipo == TipoOp::MUL || ins.tipo == TipoOp::DIV) {
                string args;
                getline(iss, args);
                
                size_t pos1 = args.find(',');
                size_t pos2 = args.find(',', pos1 + 1);
                
                if(pos1 != string::npos && pos2 != string::npos) {
                    string rd = args.substr(0, pos1);
                    string rs = args.substr(pos1 + 1, pos2 - pos1 - 1);
                    string rt = args.substr(pos2 + 1);
                    
                    trim(rd); trim(rs); trim(rt);
                    ins.dest = indiceRegistrador(rd);
                    ins.src1 = indiceRegistrador(rs);
                    ins.src2 = indiceRegistrador(rt);
                }
            } 
            // Parse para instruções de load (carregamento imediato)
            else if(ins.tipo == TipoOp::LD) {
                string args;
                getline(iss, args);
                
                size_t pos = args.find(',');
                if(pos != string::npos) {
                    string rd = args.substr(0, pos);
                    string immediate = args.substr(pos + 1);
                    
                    trim(rd); trim(immediate);
                    ins.dest = indiceRegistrador(rd);
                    ins.imm = stoi(immediate);
                }
            } 
            // Parse para instruções de store
            else if(ins.tipo == TipoOp::ST) {
                string args;
                getline(iss, args);
                
                size_t pos = args.find(',');
                if(pos != string::npos) {
                    string rs = args.substr(0, pos);
                    string endereco = args.substr(pos + 1);
                    
                    trim(rs); trim(endereco);
                    ins.src1 = indiceRegistrador(rs);
                    ins.imm = stoi(endereco);
                }
            }
            
            filaInstr.push_back(ins);
            if(filaInstr.size() >= MAX_INS - 1) break;
        }
        f.close();
    }


    int slots_livres_rob() const {
        int usado = (cauda_rob - cabeca_rob + TAM_ROB) % TAM_ROB;
        return TAM_ROB - usado - 1;
    }

    int alocar_rob() {
        int proximo = cauda_rob;
        ROB[proximo] = EntradaROB();
        ROB[proximo].ocupada = true;
        cauda_rob = (cauda_rob % TAM_ROB) + 1;
        return proximo;
    }

    EntradaROB* entrada_rob(int tag) {
        if(tag < 1 || tag > TAM_ROB) return nullptr;
        return &ROB[tag];
    }
    template<size_t N>
    int encontrar_rs_livre(array<EstacaoReserva, N>& rs) {
        for(size_t i = 0; i < N; i++) {
            if(!rs[i].ocupada) return i;
        }
        return -1;
    }

    int encontrar_buffer_carga_livre() {
        for(size_t i = 0; i < BUFFER_CARGA_COUNT; i++) {
            if(!BufferCarga[i].ocupado) return i;
        }
        return -1;
    }

    int encontrar_buffer_arm_livre() {
        for(size_t i = 0; i < BUFFER_ARM_COUNT; i++) {
            if(!BufferArm[i].ocupado) return i;
        }
        return -1;
    }

    void emitir() {
        if(pc >= (int)filaInstr.size()) return;
        
        Instr& ins = filaInstr[pc];
        
        if(slots_livres_rob() <= 0) return;
        
        // Instruções de load (carregamento imediato)
        if(ins.tipo == TipoOp::LD) {
            int idx = encontrar_buffer_carga_livre();
            if(idx < 0) return;
            
            int tag = alocar_rob();
            
            BufferCarga[idx].ocupado = true;
            BufferCarga[idx].op = TipoOp::LD;
            BufferCarga[idx].endereco = ins.imm;
            BufferCarga[idx].indice_rob = tag;
            BufferCarga[idx].Q = 0;
            BufferCarga[idx].V = 0;
            BufferCarga[idx].executando = false;
            BufferCarga[idx].ciclosExecRestantes = 0;
            BufferCarga[idx].nome = "L" + to_string(idx);
            
            EntradaROB* r = entrada_rob(tag);
            r->ocupada = true;
            r->op = TipoOp::LD;
            r->dest = ins.dest;
            r->pronta = false;
            r->valor = 0;
            r->texto_instr = ins.texto;
            
            arquivo_reg[ins.dest].tag = tag;
            pc++;
        } 
        // Instruções de store
        else if(ins.tipo == TipoOp::ST) {
            int idx = encontrar_buffer_arm_livre();
            if(idx < 0) return;
            
            int tag = alocar_rob();
            
            BufferArm[idx].ocupado = true;
            BufferArm[idx].op = TipoOp::ST;
            BufferArm[idx].endereco = ins.imm;
            BufferArm[idx].indice_rob = tag;
            BufferArm[idx].V = 0;
            BufferArm[idx].Q = 0;
            BufferArm[idx].executando = false;
            BufferArm[idx].ciclosExecRestantes = 0;
            BufferArm[idx].nome = "S" + to_string(idx);
            
            int src = ins.src1;
            if(arquivo_reg[src].tag == 0) {
                BufferArm[idx].V = arquivo_reg[src].valor;
                BufferArm[idx].Q = 0;
            } else {
                BufferArm[idx].Q = arquivo_reg[src].tag;
            }
            
            EntradaROB* r = entrada_rob(tag);
            r->ocupada = true;
            r->op = TipoOp::ST;
            r->dest = -1;
            r->pronta = false;
            r->endereco_mem = ins.imm;
            r->texto_instr = ins.texto;
            
            pc++;
        } 
        // Instruções aritméticas
        else {
            int idx;
            EstacaoReserva* rsarr;
            
            if(ins.tipo == TipoOp::MUL || ins.tipo == TipoOp::DIV) {
                idx = encontrar_rs_livre(RS_mul);
                if(idx < 0) return;
                rsarr = &RS_mul[idx];
            } else {
                idx = encontrar_rs_livre(RS_soma);
                if(idx < 0) return;
                rsarr = &RS_soma[idx];
            }
            
            int tag = alocar_rob();
            
            rsarr->ocupada = true;
            rsarr->op = ins.tipo;
            rsarr->destRob = tag;
            rsarr->indice_rob = tag;
            rsarr->executando = false;
            rsarr->ciclosExecRestantes = 0;
            rsarr->nome = "RS" + to_string(idx);
            
            // Verifica dependências para src1
            if(arquivo_reg[ins.src1].tag == 0) {
                rsarr->Vj = arquivo_reg[ins.src1].valor;
                rsarr->Qj = 0;
            } else {
                rsarr->Qj = arquivo_reg[ins.src1].tag;
            }
            
            // Verifica dependências para src2
            if(arquivo_reg[ins.src2].tag == 0) {
                rsarr->Vk = arquivo_reg[ins.src2].valor;
                rsarr->Qk = 0;
            } else {
                rsarr->Qk = arquivo_reg[ins.src2].tag;
            }
            
            EntradaROB* r = entrada_rob(tag);
            r->ocupada = true;
            r->op = ins.tipo;
            r->dest = ins.dest;
            r->pronta = false;
            r->texto_instr = ins.texto;
            
            arquivo_reg[ins.dest].tag = tag;
            pc++;
        }
    }

    int latencia_op(TipoOp t) const {
        switch(t) {
            case TipoOp::ADD:
            case TipoOp::SUB: return LAT_SOMA;
            case TipoOp::MUL: return LAT_MUL;
            case TipoOp::DIV: return LAT_DIV;
            case TipoOp::LD: return LAT_CARGA;
            case TipoOp::ST: return LAT_ARM;
            default: return 1;
        }
    }
    template<size_t N>
    void tentar_iniciar_rs(array<EstacaoReserva, N>& rsarr) {
        for(auto& rs : rsarr) {
            if(!rs.ocupada || rs.executando) continue;
            if(rs.Qj == 0 && rs.Qk == 0) {
                rs.executando = true;
                rs.ciclosExecRestantes = latencia_op(rs.op);
            }
        }
    }

    void tentar_iniciar_cargas() {
        for(auto& lb : BufferCarga) {
            if(!lb.ocupado || lb.executando) continue;
            lb.executando = true;
            lb.ciclosExecRestantes = latencia_op(TipoOp::LD);
        }
    }

    void tentar_iniciar_arms() {
        for(auto& sb : BufferArm) {
            if(!sb.ocupado || sb.executando) continue;
            if(sb.Q == 0) {
                sb.executando = true;
                sb.ciclosExecRestantes = latencia_op(TipoOp::ST);
            }
        }
    }

    void transmitir_resultado(int tag_rob, int valor) {
        // Atualiza estações de reserva de soma
        for(auto& rs : RS_soma) {
            if(rs.ocupada) {
                if(rs.Qj == tag_rob) { rs.Vj = valor; rs.Qj = 0; }
                if(rs.Qk == tag_rob) { rs.Vk = valor; rs.Qk = 0; }
            }
        }
        
        // Atualiza estações de reserva de multiplicação/divisão
        for(auto& rs : RS_mul) {
            if(rs.ocupada) {
                if(rs.Qj == tag_rob) { rs.Vj = valor; rs.Qj = 0; }
                if(rs.Qk == tag_rob) { rs.Vk = valor; rs.Qk = 0; }
            }
        }
        
        // Atualiza buffers de store
        for(auto& sb : BufferArm) {
            if(sb.ocupado && sb.Q == tag_rob) {
                sb.V = valor;
                sb.Q = 0;
            }
        }
        
        // Atualiza registradores
        for(auto& reg : arquivo_reg) {
            if(reg.tag == tag_rob) {
                reg.valor = valor;
            }
        }
    }

    void avancar_execucao_e_escrever(int ciclo) {
        // Execução nas estações de reserva de soma/subtração
        for(auto& rs : RS_soma) {
            if(rs.ocupada && rs.executando) {
                rs.ciclosExecRestantes--;
                if(rs.ciclosExecRestantes <= 0) {
                    int res = 0;
                    switch(rs.op) {
                        case TipoOp::ADD: res = rs.Vj + rs.Vk; break;
                        case TipoOp::SUB: res = rs.Vj - rs.Vk; break;
                        default: break;
                    }
                    
                    int tag = rs.indice_rob;
                    EntradaROB* r = entrada_rob(tag);
                    r->valor = res;
                    r->pronta = true;
                    
                    transmitir_resultado(tag, res);
                    rs.ocupada = false;
                    rs.executando = false;
                }
            }
        }
        
        // Execução nas estações de reserva de multiplicação/divisão
        for(auto& rs : RS_mul) {
            if(rs.ocupada && rs.executando) {
                rs.ciclosExecRestantes--;
                if(rs.ciclosExecRestantes <= 0) {
                    int res = 0;
                    switch(rs.op) {
                        case TipoOp::MUL: res = rs.Vj * rs.Vk; break;
                        case TipoOp::DIV:
                            res = (rs.Vk != 0) ? rs.Vj / rs.Vk : 0;
                            break;
                        default: break;
                    }
                    
                    int tag = rs.indice_rob;
                    EntradaROB* r = entrada_rob(tag);
                    r->valor = res;
                    r->pronta = true;
                    
                    transmitir_resultado(tag, res);
                    rs.ocupada = false;
                    rs.executando = false;
                }
            }
        }
        
        // Execução de loads (carregamento imediato)
        for(auto& lb : BufferCarga) {
            if(lb.ocupado && lb.executando) {
                lb.ciclosExecRestantes--;
                if(lb.ciclosExecRestantes <= 0) {
                    int val = lb.endereco;
                    
                    int tag = lb.indice_rob;
                    EntradaROB* r = entrada_rob(tag);
                    r->valor = val;
                    r->pronta = true;
                    
                    transmitir_resultado(tag, val);
                    lb.ocupado = false;
                    lb.executando = false;
                }
            }
        }
        
        // Execução de stores
        for(auto& sb : BufferArm) {
            if(sb.ocupado && sb.executando) {
                sb.ciclosExecRestantes--;
                if(sb.ciclosExecRestantes <= 0) {
                    int tag = sb.indice_rob;
                    EntradaROB* r = entrada_rob(tag);
                    r->pronta = true;
                    r->valor_arm = sb.V;
                    r->endereco_mem = sb.endereco;
                    
                    sb.ocupado = false;
                    sb.executando = false;
                }
            }
        }
    }

    void consolidar() {
        if(ROB[cabeca_rob].ocupada && ROB[cabeca_rob].pronta) {
            EntradaROB& r = ROB[cabeca_rob];
            
            // Instruções de store: escrever na memória
            if(r.op == TipoOp::ST) {
                int addr = r.endereco_mem;
                if(addr >= 0 && addr < TAM_MEM) {
                    memoria[addr] = r.valor_arm;
                }
            } 
            // Outras instruções: atualizar registradores
            else if(r.op == TipoOp::LD || r.op == TipoOp::ADD || 
                     r.op == TipoOp::SUB || r.op == TipoOp::MUL || 
                     r.op == TipoOp::DIV) {
                int dest = r.dest;
                if(dest >= 0 && dest < REGISTRADORES) {
                    if(arquivo_reg[dest].tag == cabeca_rob) {
                        arquivo_reg[dest].valor = r.valor;
                        arquivo_reg[dest].tag = 0;
                    }
                }
            }
            
            // Libera entrada do ROB
            r = EntradaROB();
            cabeca_rob = (cabeca_rob % TAM_ROB) + 1;
        }
    }


    void imprimir_estado(int ciclo) const {
        cout << "------------------------------------------------------------\n";
        cout << "CICLO: " << ciclo << "\n";
        cout << "PC: " << pc << " / " << filaInstr.size() << "\n";
        
        cout << "\nESTACOES DE RESERVA (ADD/SUB):\n";
        cout << "Idx | Ocup | Op  | Vj   | Vk   | Qj | Qk | ROB\n";
        for(size_t i = 0; i < RS_SOMA_COUNT; i++) {
            printf("%3zu |  %3d | %3s | %4d | %4d | %2d | %2d | %2d\n",
                i, RS_soma[i].ocupada ? 1 : 0, 
                RS_soma[i].ocupada ? nomeOp(RS_soma[i].op) : "--",
                RS_soma[i].Vj, RS_soma[i].Vk, RS_soma[i].Qj, RS_soma[i].Qk, 
                RS_soma[i].indice_rob);
        }
        
        cout << "\nESTACOES DE RESERVA (MUL/DIV):\n";
        cout << "Idx | Ocup | Op  | Vj   | Vk   | Qj | Qk | ROB\n";
        for(size_t i = 0; i < RS_MUL_COUNT; i++) {
            printf("%3zu |  %3d | %3s | %4d | %4d | %2d | %2d | %2d\n",
                i, RS_mul[i].ocupada ? 1 : 0, 
                RS_mul[i].ocupada ? nomeOp(RS_mul[i].op) : "--",
                RS_mul[i].Vj, RS_mul[i].Vk, RS_mul[i].Qj, RS_mul[i].Qk, 
                RS_mul[i].indice_rob);
        }
        
        cout << "\nBUFFERS DE CARGA (Load Immediate):\n";
        cout << "Idx | Ocup | Imm  | ROB | ExecRest\n";
        for(size_t i = 0; i < BUFFER_CARGA_COUNT; i++) {
            printf("%3zu |  %3d | %4d |  %2d | %3d\n",
                i, BufferCarga[i].ocupado ? 1 : 0, BufferCarga[i].endereco, 
                BufferCarga[i].indice_rob, BufferCarga[i].ciclosExecRestantes);
        }
        
        cout << "\nBUFFERS DE ARMAZENAMENTO:\n";
        cout << "Idx | Ocup | Endr | V | Q | ROB | ExecRest\n";
        for(size_t i = 0; i < BUFFER_ARM_COUNT; i++) {
            printf("%3zu |  %3d | %4d | %2d | %2d | %2d | %3d\n",
                i, BufferArm[i].ocupado ? 1 : 0, BufferArm[i].endereco, 
                BufferArm[i].V, BufferArm[i].Q, BufferArm[i].indice_rob, 
                BufferArm[i].ciclosExecRestantes);
        }
        
        cout << "\nROB (cabeca=" << cabeca_rob << " cauda=" << cauda_rob << "):\n";
        cout << "Idx | Ocup | Op  | Dest | Pronta | Valor | Instr\n";
        for(int i = 1; i <= TAM_ROB; i++) {
            const EntradaROB& r = ROB[i];
            if(r.ocupada) {
                printf("%3d |  %3d | %3s |  %3d |   %3d | %5d | %s\n", 
                    i, r.ocupada ? 1 : 0, nomeOp(r.op), r.dest, 
                    r.pronta ? 1 : 0, r.valor, r.texto_instr.c_str());
            }
        }
        
        cout << "\nRegistradores (valor : tag):\n";
        for(int i = 0; i < REGISTRADORES; i++) {
            printf("R%02d=%5d : t=%2d\t", i, arquivo_reg[i].valor, arquivo_reg[i].tag);
            if((i + 1) % 4 == 0) cout << "\n";
        }
        
        cout << "\nMemoria (enderecos nao-zero ate 64):\n";
        for(int i = 0; i < 64; i++) {
            if(memoria[i] != 0) {
                cout << "M[" << i << "]=" << memoria[i] << "  ";
            }
        }
        cout << "\n------------------------------------------------------------\n";
    }

    bool finalizado() const {
        if(pc < (int)filaInstr.size()) return false;
        
        for(const auto& rs : RS_soma) if(rs.ocupada) return false;
        for(const auto& rs : RS_mul) if(rs.ocupada) return false;
        for(const auto& lb : BufferCarga) if(lb.ocupado) return false;
        for(const auto& sb : BufferArm) if(sb.ocupado) return false;
        
        for(int i = 1; i <= TAM_ROB; i++) {
            if(ROB[i].ocupada) return false;
        }
        
        return true;
    }

    void executar() {
        int ciclo = 1;
        
        cout << "====== SIMULADOR TOMASULO ======\n";
        cout << "Carregadas " << filaInstr.size() << " instrucoes.\n";
        cout << "LD funciona como LI (Load Immediate)\n";
        cout << "================================\n\n";
        
        // Loop principal de execução ciclo a ciclo
        while(!finalizado()) {
            imprimir_estado(ciclo);
            
            // Estágios do algoritmo de Tomasulo
            emitir();
            tentar_iniciar_rs(RS_soma);
            tentar_iniciar_rs(RS_mul);
            tentar_iniciar_cargas();
            tentar_iniciar_arms();
            avancar_execucao_e_escrever(ciclo);
            consolidar();
            
            ciclo++;
        }
        
        // Estado final
        imprimir_estado(ciclo);
        cout << "\n====== EXECUCAO FINALIZADA em " << ciclo - 1 << " ciclos ======\n";
    }
};

int main(int argc, char** argv) {
    string arquivo = "instrucoes.txt";
    
    if(argc >= 2) {
        arquivo = argv[1];
    }
    
    cout << "Carregando arquivo de instrucoes: " << arquivo << "\n\n";
    
    SimuladorTomasulo sim;
    sim.carregarPrograma(arquivo);
    sim.executar();
    
    return 0;
}