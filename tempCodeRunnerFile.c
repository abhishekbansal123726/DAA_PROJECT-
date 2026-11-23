CE ALLOCATION");
        puts("==============================================");
        printf("Passengers: %d | Normal security lanes: %d\n",passengerCount,laneCount);
        puts("1. Show passenger data");
        puts("2. Enter passenger data");
        puts("3. Reset sample data");
        puts("4. Set normal lane count (2-10)");
        puts("5. Run complete demo and export GUI results");
        puts("0. Exit");
        if (readInt("Choose an option: ",&choice)!=1) { puts("Input closed or invalid. Exiting."); break; }
        switch (choice) {
            case 1: showPassengers(); break;
            case 2: enterPassengers(); break;