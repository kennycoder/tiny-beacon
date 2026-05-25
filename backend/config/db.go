package config

import (
	"context"
	"log"
	"os"

	"cloud.google.com/go/firestore"
)

var FirestoreClient *firestore.Client

func InitDB() {
	ctx := context.Background()
	projectID := os.Getenv("GOOGLE_CLOUD_PROJECT")
	if projectID == "" {
		log.Fatalf("GOOGLE_CLOUD_PROJECT environment variable is not set")
	}

	dbID := os.Getenv("FIRESTORE_DATABASE")
	if dbID == "" {
		log.Fatalf("FIRESTORE_DATABASE environment variable is not set")
	}

	var err error
	// If FIRESTORE_EMULATOR_HOST is set, firestore.NewClientWithDatabase automatically routes to it.
	FirestoreClient, err = firestore.NewClientWithDatabase(ctx, projectID, dbID)
	if err != nil {
		log.Fatalf("Failed to initialize Firestore client: %v", err)
	}

	log.Printf("Firestore client initialized for project %s, database %s", projectID, dbID)
}
