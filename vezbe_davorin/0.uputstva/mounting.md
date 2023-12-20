# Uputstva

1. Upaliti plocu i procitati IP adresu sa monitora.
2. Sa racunala upisati (password = root): 
    ```
    telnet <ip_ploce> 
    ```
3. Sa ploce je potrebno mountati direktorij na plocu:
    ```
    mount -o port=2049,nolock,proto=tcp -t nfs 192.168.28.13:/home/student/pputvios1/ploca /mnt/PPUTVIOS_Student1
    ```
4. Pokretanje programa:
    ```
    cd /mnt/PPUTVIOS_Student1

    ./ime_izvrsne_datoteke
    ```
